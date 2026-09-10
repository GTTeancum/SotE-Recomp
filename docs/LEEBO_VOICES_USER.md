# Leebo's PC communicator voices

Use the original recordings from your own PC installation of Shadows of the Empire. Locate the `Sdata` folder containing `ILB01.WAV`, copy its `ILB*.WAV` files to `Sdata` beside `Shadows of the Empire.exe`, and restart the game. The resulting path must be `Sdata/ILB01.WAV`, not `Sdata/Sdata/ILB01.WAV`.

Original PCM WAV files work directly. The loader supports mono/stereo 8-bit or 16-bit PCM WAV; compressed ADPCM files are not supported. Keep the original filenames. The release does not include these recordings.

There are 32 distinct mapped communicator messages. Some PC clips are shorter portions of other recordings, so every ILB file is not a separate event. Speech starts when the matching communicator text is drawn and can play again when the message reappears. The N64 text remains unchanged; PC wording and counts can differ.

Without these optional files, the game keeps its native text and sound effects. To troubleshoot, check that the WAV files are directly inside the correct `Sdata` folder and inspect the latest logs under `logs`.
