---
name: Unexpected Query Result
about: Create a report to help us improve our engine
labels: bug
---

**Observed query result**
<!-- Describe the actual, undesired query result. -->

**Expected query result**
<!-- Describe the expected query result. -->

**Steps to reproduce**
<!-- Describe the steps that lead to the unexpected behavior, e.g., what data you ingested and what commands you ran to reproduce the unexpected query result. -->

**Additional Information**
<!-- The following survey helps developers triage your issue. Please fill out the *Required* section, and add what feels related in the *Optional* section. -->

*Required*
- Output of `vast version`
- Contents of your configuration files, e.g., `PREFIX/etc/vast/vast.yaml`
- Environment the error was reproduced in (e.g., `macOS 10.15.5`, or Docker `debian-buster`)

*Optional*
<!-- The following steps are optional but give us extra context that helps us reproduce the issue locally. -->

- Verify that the issue can be reproduced after restarting the client.
- Verify that the issue can be reproduced after the server and client.
- Check if the issue can be reproduced after removing `vast.db/` (make a backup)
- When building from source, provide the build summary, which the `configure` step displays at the end. Please also include the contents of the file `config.status` in the build directory.
- Run `vast` with --verbosity=debug and show the additional log output on both the server and client-side.
- If possible, upload the `vast.db/server.log` file, or if the data may be shared (e.g., for small test instances) a tarball of your `vast.db` folder.
