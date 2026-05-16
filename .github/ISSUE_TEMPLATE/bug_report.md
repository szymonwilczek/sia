---
name: Bug Report
about: Report numeric or mathematical bug in the SIA engine.
title: 'bug: [short title of bug]'
labels: bug
assignees: ''
---

<!--
Every placeholder is marked as note (`>`). Before submitting an issue, please delete/replace it with your text.
-->


### Context
    Provide context here (delete this placeholder).

### Examples of Current Behavior
    Provide examples of current behavior (delete this placeholder), for example:
`sia> lim(1/x, x, 0)`
*Current output:* `inf` (Generic output, masking the essential singularity and lack of two-sided convergence)

`sia> lim(1/x, x, 0+)`
*Current output:* `error: unexpected token: RPAREN`

### Expected Behavior
    Provide examples of current behavior (delete this placeholder), for example:
`sia> lim(1/x, x, 0+)`
*Expected output:* `inf`

`sia> lim(1/x, x, 0-)`
*Expected output:* `-inf`

### Environment & Version
    Below is the placeholder (so as this, fill it please):
- **SIA version**: `x.x.x-x`
- **Compiler & OS**: `gcc 13.2`, `Fedora Linux`
