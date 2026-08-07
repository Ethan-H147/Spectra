# Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).

## Project

- Source repository: [Ethan-H147/Spectra](https://github.com/Ethan-H147/Spectra)
- Published releases: [Spectra releases](https://github.com/Ethan-H147/Spectra/releases)
- License: [MIT](LICENSE)
- Privacy policy: [PRIVACY.md](PRIVACY.md)
- Security policy: [SECURITY.md](SECURITY.md)

## Roles

Spectra currently has one maintainer.

- Committer and reviewer: [Ethan-H147](https://github.com/Ethan-H147)
- Signing approver: [Ethan-H147](https://github.com/Ethan-H147)

The maintainer must use multi-factor authentication for GitHub and SignPath before requesting signatures. If the project adds maintainers, this policy will identify their roles before they can request or approve signatures.

## Signed artifacts

Spectra signs only first-party release binaries built from this repository. Bundled upstream open-source libraries retain their upstream signatures or remain unsigned. Spectra does not apply its signing identity to upstream binaries.

The signing pipeline covers:

- the Spectra application executable;
- the Windows installer;
- the generated uninstaller; and
- future first-party executable or library files distributed with Spectra.

## Release requirements

A signing request must satisfy all of these conditions:

1. The source commit belongs to the `main` branch.
2. The version tag matches the version in `CMakeLists.txt`.
3. GitHub Actions builds the release from the tagged source commit.
4. All automated tests pass.
5. The signing approver manually approves the SignPath request.
6. The pipeline verifies each expected Authenticode signature before publication.
7. Published artifacts include SHA-256 checksum files.

Release signatures use SHA-256 and a trusted RFC 3161 timestamp. Published assets remain immutable. A changed binary requires a new version and tag.

## Build and key controls

GitHub Actions builds release artifacts from the public source and pinned dependency definitions. SignPath stores signing keys in its managed signing service. Spectra does not store a private signing key or certificate password in the repository or release workflow.

Every signing request requires manual approval. Pull requests and ordinary branch builds cannot publish signed releases.

## Compromise and revocation

The maintainer will stop signing and publishing releases after evidence of a compromised repository, workflow, maintainer account, or signing process. The maintainer will notify SignPath, investigate affected releases, request certificate revocation when required, and publish remediation details through a GitHub security advisory.
