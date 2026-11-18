# AGENTS.md

## How to work

- Never fix anything the quick way. Before changing code, silencing a warning (`NOLINT`, a cast, a config option) or proposing a change, investigate in depth why the problem happens, reason about it, and verify the conclusion against the code. Only then propose or apply the fix.
- A suppression is the last resort, used only once the cause is understood and it is proven that no real fix exists. If similar code does not trigger the same warning or problem, find the difference before doing anything: it is usually the real cause.
- Never propose something that has not been verified (types, callers, whether it can actually be implemented). A plausible guess presented as a conclusion is worse than saying it has not been checked yet.

## Scope

These rules apply to every C++ source file and header of this project, that is, the files matched by the `CLANG_FORMAT_PATHS` constant in `scripts/clang-scripts.mjs` (`src/`, `include/`, `test/` and `demo/`).

Every rule states a criterion. The cases, names and snippets that come with a rule illustrate that criterion, they never delimit it: a line that does not literally match any of them is judged by the criterion, never treated as uncovered because it is not listed.

These rules are kept in sync with [mediasoup](https://github.com/versatica/mediasoup)'s `worker/AGENTS.md`. Every rule there applies here, except the ones about code that does not exist in this project (`common.hpp`, the `MS_` macros, timers, libuv, flatbuffers, `Channel`, mocks and fuzzers) and the single exception listed in the last section. When a rule of the worker is not in this file, it has been forgotten: ask instead of assuming that it does not apply.

## Building and testing

Read [doc/Building.md](doc/Building.md) for the whole workflow. In short:

- Build with `cmake --build build`. The build directory must have been configured with the libwebrtc paths and `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.
- Run the tests with `./scripts/test.sh`, or directly with the already built binary (`./build/test/test.app/Contents/MacOS/test` in macOS).
- `npm run lint` and `npm run tidy` validate style and checks. In macOS, `npm run tidy` needs `SDKROOT` exported (`export SDKROOT=$(xcrun --show-sdk-path)`), otherwise every file fails with "'algorithm' file not found".
- A change is not finished until `npm run lint`, `npm run tidy`, the build and the tests pass. Run all of them, and never claim that any of them passes without having run it.
- `npm run tidy:fix` is a tool, not an authority: its fixes are verified by building and running the tests. It has already produced wrong code here (it turned `std::for_each(encodings->begin(), encodings->end(), ...)` into `std::ranges::for_each(encodings, ...)`, passing the pointer instead of `*encodings`).
- Inserting includes or `const` makes clang-format realign neighbouring lines, so run `npm run format` after `npm run tidy:fix` and check that the resulting diff is only what was intended.

## Linter configuration

- `.clang-format` and `.clang-tidy` are kept in sync with mediasoup's `worker` folder. Do not change a setting here that could be changed there instead: update it in mediasoup first and copy the file.
- The only accepted exceptions are settings that name paths or types of this project (`HeaderFilterRegex`, `IncludeCategories`, the `nlohmann::basic_json` exclusion), plus `GlobalFunctionCase`, which carries a TODO (see "Naming"). Each one carries a comment in the file saying why, and is listed in `doc/Building.md`.
- `test/.clang-tidy` inherits the root configuration and disables what the Catch2 macros trigger.
- Every suppression is a `NOLINTNEXTLINE(check)` (or `NOLINTBEGIN`/`NOLINTEND` for a block), never a trailing `// NOLINT(check)`.
- A suppression carries a `// NOTE:` above it only when the reason is not evident from the lines around it, and then the note says what makes the warning wrong there (`// NOTE: The regex always has these 2 groups when it matches.`), never something that adds nothing (`// NOTE: Checked manually.`). Most suppressions need no note: in mediasoup's worker only 3 of the 88 `cppcoreguidelines-pro-bounds-avoid-unchecked-container-access` and 1 of the 24 `bugprone-throwing-static-initialization` carry one.

## Class members and constructors

- A member that the constructor must set gets no default initializer in the `.hpp`, so that the compiler complains if a constructor forgets it.
- A member whose value comes verbatim from a constructor argument is set in the member initializer list, never assigned in the constructor body.
- If every constructor is guaranteed to set a member, whether in the initializer list or in its body, the member gets no default initializer in the `.hpp`: the constructor always overwrites it, so a default would be dead code.
- A member that not every constructor sets (several constructors, or a default constructor) keeps its default initializer, unless its type already default-constructs to a defined value (a class or struct whose fields all have default initializers, `std::string`, containers, smart pointers...). Then no `{}` is added, since it would change nothing.
- A member whose initial value is a constant that does not depend on any argument gets that value as its initializer in the `.hpp`, not in the initializer list.
- A `std::optional` member that starts empty gets no initializer at all: `std::optional<uint16_t> frameNumber;`, never `{ std::nullopt }`, which is what it already is.
- The initializer list follows the declaration order of the members. Never reorder members to fit the initializer list.
- `explicit` goes only on constructors that can be called with a single argument (one parameter, or several with all but the first defaulted), never on one that takes none or that always takes several.

## `const` members

- Every member that is set in the constructor and never written again is `const`: `const T x` for values, `T* const x` for pointers (including `listener`), `const std::unique_ptr<T>` for owned objects, `const std::function<...>` for callbacks.
- Before making a member `const`, check that it is not written from subclasses, not passed by non-const reference, and not mutated through non-const methods.
- Do not make members `const` in value types that are copied, assigned, swapped or stored in containers that need assignment (`std::vector` erase/insert, `std::optional` reassignment, etc.).
- Making a member `const` never moves it: it stays where it was in the class, with `const` added and nothing else.

## Ownership

- New code avoids manual memory management: no `new`/`delete` on raw pointers. Whatever is allocated, or returned as an owning raw pointer by a factory, goes straight into a `std::unique_ptr` (`const` unless it is later `reset()`), created with `std::make_unique()` when it is our own allocation. This applies everywhere: classes, functions and tests. The only exception is the one in the last section.
- An object owned by a class is held in a `std::unique_ptr`, never as a raw pointer with `new`/`delete`.
- If it is created once in the constructor and never replaced, the `std::unique_ptr` is `const` and is created in the initializer list (a ternary is fine when it is optional and the condition is known at construction).
- If it is created lazily, conditionally with complex logic, or replaced during the object's life, the `std::unique_ptr` is not `const` and is set with `reset()` or `std::make_unique()`.
- A destructor does not `clear()` containers nor `reset()` smart pointers that the destruction of the members empties or destroys right afterwards anyway. Before removing one, follow every chain of calls that destroying that member starts, to its end: the destructors of the owned objects, of their own members, and everything any of them triggers (calls to listeners or to any other object, callbacks), and then whatever those calls trigger in turn. Every object that any link of a chain touches must still be alive at the moment the chain runs. Moving a destruction changes that moment:
  - Implicit destruction happens after the destructor body, in reverse declaration order, so by then everything the body destroyed and every member declared after this one is already gone. In a base class, the subclass is gone too.
  - `std::unique_ptr::reset()` nulls the pointer before destroying the object, unlike `delete` followed by `= nullptr`, so a chain that reads that same pointer finds it null.
- Constructors have side effects too, so replacing an owned object is not order free. Assigning a new `std::unique_ptr` over an existing one (`ptr = std::make_unique<T>(...)`, `map[key] = std::make_unique<T>(...)`) constructs the new object while the old one is still alive and destroys the old one only afterwards. Whenever code is changed so that an object is created or destroyed at another moment, check what both its constructor and its destructor register, release or trigger, and keep the original order, with a comment saying why.
- Where destruction order matters, keep an explicit `reset()` in the destructor at the place where the `delete` would have been, with a comment saying why it must go there.

## Includes

- Every `.hpp` uses include guards (`MSC_XXX_HPP`), never `#pragma once`.
- A `.cpp` never re-includes a header that its paired `.hpp` already includes.
- A type newly used in a file gets its own `#include`. Never rely on transitive includes (except through the paired `.hpp`).
- Whenever code is touched, check the includes of the file: none missing and none left over.
- A standard header that provides functions or utilities rather than a type of its own name (`<cstring>`, `<cstdlib>`, `<cmath>`, `<limits>`, `<sstream>`...) carries a comment listing what is used from it, with `()` for functions: `#include <cstring> // std::memcmp(), std::memcpy()`. Headers named after the type they provide (`<vector>`, `<string>`, `<map>`...) take no comment.
- The order of the includes is decided by `IncludeCategories` in `.clang-format` (project headers, `sdp/`, `json`/`sdptransform`, libwebrtc, standard headers) and applied by `npm run format`. Never group them by hand: a blank line inside the include block stops clang-format from sorting across it, so there are none.
- A header included only for the declarations it re-exports (`mediasoupclient.hpp`, `PeerConnection.hpp` in the tests) is marked with `// IWYU pragma: keep` and a `// NOTE:` saying what it provides, so that clangd's include cleaner does not propose removing it. A header whose whole purpose is re-exporting (`mediasoupclient.hpp`) wraps its includes in `// IWYU pragma: begin_exports` / `end_exports`. Before removing an include that clangd reports as unused, check that nothing in the file uses a declaration that it re-exports.

## Class layout

- Class constants (`static constexpr` members) go at the very top of the class body. Nested types (`struct`, `class`, `enum`, `using`) go right after them, before any method, under their own access specifier. A class constant whose type uses a nested type goes after that nested type.
- `static const` members (lookup tables such as `PeerConnection::iceConnectionState2String`...) are not class constants for this rule: they go together with the rest of the static data members, in the block right after the static methods.
- A block of `static constexpr` members always starts with its own `public:` or `private:` line, even when the previous block has the same access.
- Struct fields that need documentation use multiline `/** ... */` comments, never `//`.
- Public methods and class constants are documented with `/** ... */`. Parameter docs take a dash: `@param routerRtpCapabilities - Capabilities of the router.`
- Every method declaration is separated from the previous one by a blank line, whether it is documented or not, constructors, destructors and deleted copy/move declarations included. In a documented one the blank line goes before the `/** ... */`, which therefore never starts on the line right after another declaration. The only run that may stay packed is one of closely related one liner private helpers.
- Data members are the opposite: consecutive members stay packed, and a documented field keeps its `/** ... */` right under the previous one. A blank line in a block of members separates groups of them, never single members.
- `@remarks` goes alone on its line (no colon, no text after it), and its content goes on the following lines as dash items (`- `), with continuation lines indented two spaces.

## Functions and statements

- Local declarations inside a function or at file scope: `constexpr` first, then `const`, then non-const, with a blank line between the `const` and the non-const groups. A declaration that starts with `const` belongs to the `const` group, even if only its pointee is `const` (`const uint8_t* ptr`). This also applies when a line is made `const`: it moves up into the `const` group. The only exception is a real reason, logical or aesthetic, to declare a non-const first: the "parent" is declared first and then the "children" that are put into it or assigned to it, and a declaration that only makes sense next to the one it relates to stays there.
- None of the above applies to the private/protected members of a class in the `.hpp`: there members go in the order that makes sense to whoever reads the class, grouped by what they are under their group comment, and a `const` member stays in its place with `const` added and nothing else. It is not moved up, not separated with a blank line and not taken out of its group.
- Code is split into paragraphs with blank lines, one per step: a declaration is not glued to the first thing done with it, an action is not glued to the assert/`REQUIRE` that checks it, and a check is not glued to the next action. Whatever belongs to the same step stays together, for example several consecutive calls on the same object, the assignments that fill its fields, or several consecutive `REQUIRE`s.
- A declaration, `const` or not, is separated by a blank line from whatever is done with it afterwards, including the assignments that fill its fields.
- A statement that leaves the block (`return`, `break`, `continue`, `throw`, and any macro that throws, such as `MSC_THROW_ERROR()` and its variants) always has a blank line before it, unless it is the first statement of its block.
- A declaration that carries its own comment is a paragraph: it is separated by a blank line from the next commented declaration.
- Everything that can be `const` is `const`, including `const auto` locals and `const T&` parameters.
- A pointer parameter whose pointee the function does not modify is `const T*`. The pointer itself is never made `const` in a parameter (`T* const`): that `const` is not part of the signature and tells the caller nothing.
- In a listener callback, the pointer to the object emitting it, and to any entity that the listeners manage (`Producer`, `Consumer`, `Transport`...), is never `const T*`: the emitter cannot assume what the listener, usually its parent and owner, wants to do with it, which may be something non-const, such as closing the object. The other pointer parameters of a listener callback, data that the emitter owns or passes along, are `const T*` whenever possible.
- `auto` or an explicit type is decided by one criterion: can whoever reads the line tell the type from the line itself? If yes, `auto`. If not, spell the type out. The line shows the type when the right-hand side names it (a cast, `std::make_unique()`, a function or method whose name says what it returns), or when the name of the variable, read together with what it is assigned from, makes the type obvious. Iterators and similar types whose spelled-out name adds nothing for the reader are always `auto`.
- A method of the class itself is called without `this->`: `Close()`, never `this->Close()`. `this->` is only for members: `this->remoteSdp`.
- A `static` constant of a class declared in its `.hpp` always carries the class prefix, in every mention, also inside the class's own methods: `Logger::BufferSize`, never `BufferSize`.
- Never reach through an iterator (`it->second.field`): bind a named reference or copy first.
- Never use `.first` / `.second` inline: bind them to a named variable first (`const bool inserted = set.insert(x).second;`).
- Use a structured binding whenever both members of a pair or tuple are used: `for (const auto& [mid, idx] : this->midToIndex)`, never `const auto& mid = kv.first;` plus `const auto& idx = kv.second;`.
- A loop over every element of a container is range-based. An index or an iterator stays only when the loop needs it: to index another container in parallel, to erase while iterating, to cover part of the range, or to use the position itself.
- Aggregates are built with designated initializers: `{ .idx = idx, .reuseMid = mid }`, never `{ idx, mid }`.
- Never write the `struct` keyword before a variable, parameter or return type.
- Every lambda states its return type explicitly, `-> void` included. Hence a lambda never takes `auto` parameters: spell out the parameter type.
- Integer literals have no unsigned suffix (`0`, not `0u`), unless the suffix is needed, as in `auto idx{ 0U }` compared against `size()`. When needed it is uppercase.
- Never `using namespace`: qualify the names (`mediasoupclient::ortc::canSend()`).
- Write `Utils::Foo`, never `::Utils::Foo`.
- Access `std::optional` with `.value()`, not `*opt`.
- In production code, before touching a `.value()`, read upwards until finding what guarantees the value. If anything earlier in the code path guarantees it (an `if`, an `else`, an early `return`, an earlier assignment or anything else), it is left exactly as it is: no `NOLINT`, no local variable pulled out, no restructuring, and no mention of it. Only accesses that are really unguarded are acted upon. The `NOLINT` rule for optionals is a test rule (see "Tests (Catch2)").
- A function whose result the caller must check is declared `[[nodiscard]]`, so that ignoring it fails at compile time.
- Use the `std::ranges` version of an algorithm whenever there is one, passing the container itself when the algorithm covers all of it.
- `catch` by reference (`catch (const std::exception& error)`), never by value.
- Containers are checked with `.contains()`, not with `.find() != .end()`.
- Never `&foo` to take an address: use `std::addressof(foo)`.
- A function that must not let an exception escape (a thread entry point, `main()`) handles them all. Do not rely on a handler that can itself throw.
- Classes that are only used in one `.cpp` go into an anonymous namespace.

## Naming

- Classes, structs and methods are `CamelCase`. Members, local variables and parameters are `camelBack`. Value getters are `GetXxx()`, boolean getters `IsXxx()` / `HasXxx()`.
- Namespace scope functions are not uniform in this project: `mediasoupclient::Utils` uses `CamelCase` while `mediasoupclient::ortc` and `mediasoupclient::Sdp::Utils` use `camelBack`. `.clang-tidy` accepts both and carries a TODO about unifying them. Follow the file being edited, and do not rename any of them as part of an unrelated change.
- The `json` alias is `using nlohmann::json;` in `.cpp` files, and `using json = nlohmann::json;` in headers (with its `NOLINT`, since a using declaration there would leak the name into the global namespace of every file including them).

## Robustness

- No input coming from the application or from the network may crash the library. Inputs are validated at the boundary (`ortc::validateXxx()`) and rejected with `MSC_THROW_TYPE_ERROR()`.
- Never use `assert()`: it disappears in release builds, so an invariant it checks is not checked in production.

## Logging and errors

- Every `.cpp` that uses the logging macros defines `MSC_CLASS` before including `Logger.hpp`. A file that includes it only for the `Logger` class itself (such as `test/src/tests.cpp`, which just sets the log level) does not need it.
- Every function and method defined in a `.cpp` starts with `MSC_TRACE();`. The methods of `Logger` are the exception, since they are the logging machinery that `MSC_TRACE()` itself uses, and tracing `DefaultLogHandler::OnLog()` would recurse forever.
- Log with `MSC_DEBUG()`, `MSC_WARN()` and `MSC_ERROR()`. Prefer `MSC_WARN()` over `MSC_DEBUG()` for conditions worth seeing in production.
- Throw with `MSC_THROW_ERROR()`, `MSC_THROW_TYPE_ERROR()`, `MSC_THROW_INVALID_STATE_ERROR()` and `MSC_THROW_UNSUPPORTED_ERROR()`, never with a bare `throw` (the only `throw` statements live in the definition of those macros in `MediaSoupClientErrors.hpp`). To rethrow from a `catch`, use `throw;`, never `throw error;`, which copies and slices the exception to the static type of the handler.

## Comments

- All code text is in English, and ASCII `->` is used, never the Unicode arrow.
- A comment must help whoever reads the file cold. Do not write comments that only record a discussion, and do not add references to projects the code may have been ported from. References already in the code stay.
- Never turn a point raised while working on a change into a comment: why a header was chosen over another, why something is measured one way and not another, what an earlier version did wrong. If the code is right, it needs no note defending it.
- No semicolons in log messages or comments: use a comma in logs and split into sentences in comments.

## Tests (Catch2)

- Tests live in `test/src`, use `TEST_CASE` / `SECTION`, and are built with `MEDIASOUPCLIENT_BUILD_TESTS`.
- Helpers are lambdas defined inside the `TEST_CASE` body, never file level `static` functions or variables. What several test files share lives in `test/include` (`FakeTransportListener.hpp`, `fakeParameters.hpp`, `helpers.hpp`, `MediaStreamTrackFactory.hpp`) and is used from there instead of being written again.
- A `TEST_CASE` or `SECTION` name is a single string literal, never split into adjacent literals.
- A test expectation states what the code must do. Never change an expected value, widen a tolerance or loosen an assertion just because the code now produces something else.
- Check what an assertion actually compares. This repository had three `REQUIRE()`s comparing a counter with its own pre-increment (`x.onProduceExpectedTimesCalled == ++x.onProduceExpectedTimesCalled`), which passed without testing anything.
- Accessing a `std::optional` after `REQUIRE(opt.has_value())` needs `// NOLINTNEXTLINE(bugprone-unchecked-optional-access)`. The check flags all three forms, `*opt`, `opt->field` and `opt.value()`: using `.value()` does not avoid it, and the `REQUIRE()` above does not help because the checker does not see through the Catch2 macro.
- When there are several accesses to the same optional, bind the value once with a single `NOLINT` instead of repeating it per access.
- Before calling a test file done, search it for `.value()`, `->` and `*` on optionals and check one by one that each access has its `NOLINT` or comes from an already bound value.
- Use the existing helpers (`test/include/FakeTransportListener.hpp`, `fakeParameters.hpp`, `MediaStreamTrackFactory.hpp`) instead of writing new ones.

## The only rule of mediasoup's worker that does not apply here

- Ownership through `std::unique_ptr` with no raw `new`/`delete`, **and only for the objects that the public API hands to the application**: those returned by `Device::CreateSendTransport()` / `CreateRecvTransport()` and by `Transport::Produce()` / `ProduceData()` / `Consume()` / `ConsumeData()`. They are created with `new` and returned as owning raw pointers, and changing that would break every application using this library.

  Everywhere else the rule does apply, so the rest of the code owns through `std::unique_ptr` created with `std::make_unique()` (`PeerConnection`, `SendHandler`, `RecvHandler`, `RemoteSdp`, the `MediaSection` instances it holds...). There is no `delete` left in the library: do not add one, and do not introduce a raw owning pointer for anything that does not cross the API.
