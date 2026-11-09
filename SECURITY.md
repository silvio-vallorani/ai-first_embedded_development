# Security policy

## Reporting a vulnerability

Report vulnerabilities privately through GitHub: **Security → Report a vulnerability** on this repository ([private vulnerability reporting](https://github.com/silvio-vallorani/ai-first_embedded_development/security/advisories/new)). Do not open a public issue.

Include the affected component and version (`git describe` or the image version), the board and configuration, steps to reproduce and the impact you expect.

## What happens next

| Step | Target |
|---|---|
| Acknowledgement | 5 working days |
| Initial assessment (severity, affected versions) | 15 working days |
| Fix or mitigation for high and critical issues | 90 days, earlier when actively exploited |
| Public advisory | When a fix is available, coordinated with the reporter |

Reporters are credited in the advisory unless they ask otherwise.

## Supported versions

Security updates are provided for **5 years from the release date** of each production release, free of charge. Development builds (images signed with the public NXP development key, see ADR-0014) are not supported releases and must not be deployed.

| Version | Supported |
|---|---|
| Production releases | 5 years from release |
| Development builds | No |

No production release exists yet.

## Third-party components

Vulnerabilities in the MCUXpresso SDK, MCUboot, mbed TLS, littlefs or other components are tracked through the firmware SBOM (`docs/compliance/VULNERABILITY_HANDLING.md`). Report upstream issues to the respective maintainers as well; we coordinate the update of the affected component.

## Regulatory reporting

Actively exploited vulnerabilities and severe incidents affecting a product on the EU market are reported under the Cyber Resilience Act, Art. 14, as described in `docs/compliance/VULNERABILITY_HANDLING.md`.
