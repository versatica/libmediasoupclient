# Security Policy

## Supported Versions
| Version | Supported          |
| ------- | ------------------ |
| 3.x.x   | ✅ Active support |
| 2.x.x   | ⚠️ Critical fixes only |
| < 2.0   | ❌ Unsupported     |

## Reporting a Vulnerability

**Please do NOT publicly disclose security issues.** Follow this responsible disclosure process:

1. **Private Reporting**: Email security vulnerabilities to xxx with:
   - Project name ("libmediasoupclient") in subject line
   - Detailed description of the vulnerability
   - Proof-of-concept code or steps to reproduce
   - Impact assessment
   - Any mitigation ideas

2. **Response Expectations**:
   - Acknowledgement within 3 business days
   - Investigation update within 7 business days
   - Patch timeline mutually agreed upon

3. **Public Disclosure**:  
   Coordinated after patching through:
   - GitHub Security Advisories
   - Release notes with CVE identifier
   - xxx mailing list

## Vulnerability Handling Process
1. Confirmation of reported issue
2. CVE assignment request
3. Patch development on private branch
4. Internal security review
5. Coordinated release including:
   - Patched library versions
   - Security advisory
   - Documentation updates

## Security Practices
- Regular dependency scanning (Dependabot integration)
- Security-focused code reviews
- Fuzzing tests for media processing components
- Signed release artifacts (Git tags)
- Sandboxed testing environment

## Security Acknowledgments
View our [Hall of Fame](ACKNOWLEDGEMENTS.md) for researchers who've helped improve libmediasoupclient's security.
