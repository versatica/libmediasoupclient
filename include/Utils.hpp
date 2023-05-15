#ifndef MSC_UTILS_HPP
#define MSC_UTILS_HPP

#include <cstdint> // uint32_t
#include <ctime>   // generator seed
#include <random>  // generators header
#include <set>
#include <sstream> // istringstream
#include <string>
#include <vector>

namespace mediasoupclient
{
	namespace Utils
	{
		template<typename T>
		T getRandomInteger(T min, T max);
		std::string getRandomInteger(size_t len);

		std::vector<std::string> split(const std::string& s, char delimiter);
		std::string join(const std::vector<std::string>& v, char delimiter);
		std::string join(const std::vector<uint32_t>& v, char delimiter);

		// https://stackoverflow.com/a/447307/4827838.
		bool isInt(const std::string& str);
		bool isFloat(const std::string& str);
		int toInt(const std::string& str);
		float toFloat(const std::string& str);

		/* Inline utils implementations */

		template<typename T>
		inline T getRandomInteger(T min, T max)
		{
			// Seed with time.
			static unsigned int seed = time(nullptr);

			// Engine based on the Mersenne Twister 19937 (64 bits).
			static std::mt19937_64 rng(seed);

			// Uniform distribution for integers in the [min, max) range.
			// https://en.cppreference.com/w/cpp/numeric/random/uniform_int_distribution, [min, max]
			std::uniform_int_distribution<T> dis(min, max - 1);

			return dis(rng);
		}

		inline std::string getRandomString(size_t len)
		{
			/* clang-format off */
		static std::vector<char> chars =
		{
			'0','1','2','3','4','5','6','7','8','9',
			'a','b','c','d','e','f','g','h','i','j',
			'k','l','m','n','o','p','q','r','s','t',
			'u','v','w','x','y','z',
			'A','B','C','D','E','F','G','H','I','J',
			'K','L','M','N','O','P','Q','R','S','T',
			'U','V','W','X','Y','Z'
		};
			/* clang-format on */

			// Seed with time.
			static unsigned int seed = time(nullptr);

			// Engine based on the Mersenne Twister 19937 (64 bits).
			static std::mt19937_64 rng(seed);

			// Uniform distribution for integers in the [min, max) range.
			std::uniform_int_distribution<std::string::size_type> dis(0, chars.size() - 1);

			std::string s;

			s.reserve(len);

			while ((len--) != 0u)
				s += chars[dis(rng)];

			return s;
		}

		inline std::vector<std::string> split(const std::string& s, char delimiter)
		{
			std::vector<std::string> tokens;
			std::string token;
			std::istringstream tokenStream(s);
			while (std::getline(tokenStream, token, delimiter))
			{
				tokens.push_back(token);
			}
			return tokens;
		}

		inline std::string join(const std::vector<std::string>& v, char delimiter)
		{
			std::string s;

			auto it = v.begin();
			for (; it != v.end(); ++it)
			{
				s += *it;
				if (it != v.end() - 1)
					s += delimiter;
			}

			return s;
		}

		inline std::string join(const std::vector<uint32_t>& v, char delimiter)
		{
			std::string s;

			auto it = v.begin();
			for (; it != v.end(); ++it)
			{
				s += std::to_string(*it);
				if (it != v.end() - 1)
					s += delimiter;
			}

			return s;
		}

		inline bool isInt(const std::string& str)
		{
			std::istringstream iss(str);
			int64_t l;

			iss >> std::noskipws >> l;

			return iss.eof() && !iss.fail();
		}

		inline bool isFloat(const std::string& str)
		{
			std::istringstream iss(str);
			float f;

			iss >> std::noskipws >> f;

			return iss.eof() && !iss.fail();
		}

		inline int toInt(const std::string& str)
		{
			std::istringstream iss(str);
			int64_t ll;

			iss >> std::noskipws >> ll;

			if (iss.eof() && !iss.fail())
				return std::stoll(str);

			return 0;
		}

		inline float toFloat(const std::string& str)
		{
			std::istringstream iss(str);
			double d;

			iss >> std::noskipws >> d;

			if (iss.eof() && !iss.fail())
				return std::stod(str);

			return 0.0f;
		}
	} // namespace Utils
} // namespace mediasoupclient

#endif
