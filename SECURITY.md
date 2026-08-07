# Security policy

## Supported releases

Security fixes apply to the latest published Spectra release. Users should reproduce a suspected vulnerability with the latest release before reporting it when safe to do so.

## Reporting a vulnerability

Do not disclose an unpatched vulnerability in a public issue. Submit a private report through [GitHub private vulnerability reporting](https://github.com/Ethan-H147/Spectra/security/advisories/new).

Include:

- the affected Spectra version;
- the Windows version and architecture;
- the input format and minimum reproduction steps;
- the expected and observed behavior;
- the security impact; and
- relevant logs, crash details, or proof-of-concept files.

Remove personal audio and private paths from reports. Use a synthetic audio file when the input affects reproduction.

The maintainer will acknowledge a complete report, investigate it, and coordinate disclosure after a fix is available. The response time depends on severity and reproduction quality.

## Release integrity

Published releases include SHA-256 checksum files. Signed releases also include Authenticode signatures that must pass Windows signature verification. The [code-signing policy](CODE_SIGNING_POLICY.md) defines release approval and signing controls.
