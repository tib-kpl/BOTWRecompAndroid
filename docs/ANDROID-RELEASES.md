# Android CI releases

The [Android release workflow](../.github/workflows/android-release.yml) compiles an APK on every branch push and
pull request. Every successful push to `main` also publishes a separate GitHub prerelease with a signed arm64-v8a
APK and `SHA256SUMS.txt`. A push containing several commits builds its tip. Runs are not cancelled by later pushes.
You can also start a build from Actions → Android release → Run workflow.

Release tags contain the workflow run number and commit hash. Re-running the same workflow replaces that release's
assets. CI uses version codes starting at 1001, increasing with the workflow run number, so newer APKs can be
installed over older CI APKs. The version name includes the build number and commit hash. These automated builds
are prereleases; they do not replace a manually published stable release.

## Signing

The main-branch build requires these repository Actions secrets:

| Secret | Value |
| --- | --- |
| `ANDROID_KEYSTORE_BASE64` | Base64 encoding of the persistent release keystore |
| `ANDROID_KEYSTORE_PASSWORD` | Keystore password |
| `ANDROID_KEY_ALIAS` | Signing key alias |
| `ANDROID_KEY_PASSWORD` | Key password |

Back up the keystore and its passwords securely. If a new CI key is created locally, its backup is in
`.android-signing/`, which is ignored by Git. Never commit it. Keep using the same key for future releases. An APK
signed by a different key cannot update an existing installation signed by the old key.

For a local signed build, set `BOTW_KEYSTORE_PROPERTIES` to an absolute path to a Java properties file containing
`storeFile`, `storePassword`, `keyAlias`, and `keyPassword`. Otherwise Gradle uses
`~/.botwrecompandroid/keystore.properties`. Add `-PrequireReleaseSigning=true` to reject a missing signing file,
as CI does. Non-main branches and pull requests only build debug APKs and never receive release signing secrets.

## Build contents

The workflow uses Java 17, SDK 36, NDK `30.0.16248370`, CMake `3.30.4` and the checked-in Gradle wrapper. It builds
SDL, shaderc, FFmpeg, and the Python/Zig/Capstone/Unicorn/LZ4 toolchain needed to recompile the player's game on the
phone. Native dependencies are cached by their build scripts and NDK version. No game dump is needed or included.
Before publishing, CI verifies the APK signature and checks that the native runtime and on-device compiler are
actually packaged. A failed build publishes no release; inspect the Actions log for the failing step.
