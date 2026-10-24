# 7-Zip language files

The `Lang/` folder of the official 7-Zip release, unchanged: one file per
language (`pl.txt`, ...) and `en.ttt`, the English template for
translators. The upstream source repository does not have them; they come
with the binary release, under the GNU LGPL like the rest of 7-Zip (see
`DOC/License.txt`).

Source: 7-Zip 26.05 for Windows x64, `7z2605-x64.exe`
(https://www.7-zip.org/a/7z2605-x64.exe), SHA-256
`fd99e86f57d8f429ef5ed045bf98315af962e2309b0f1ba87a147a44c71647d0`.

7zFM and 7zG read them from `Lang/` in their program folder
(`FileManager/LangUtils.cpp`): the build copies them next to both programs.
Without a choice in Tools > Options > Language, 7-Zip takes the language
of the locale (`linux/platform/Locale.cpp`). After merging a new upstream
release, replace these files with the ones of the matching Windows
release.
