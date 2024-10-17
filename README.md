# blubblaunch
This is a macOS native launcher for universal Java Applications, i.e. those that run natively with both ARM (Apple Silicon, M-series CPUs) and Intel (x86_64 CPUs). It is a replacement for the JavaApplicationStub of old. It is not a packager, but rather intended for those who create Mac applications in their own buld pipelines.

# Necessary for Sequoia
This project exists because under Sequoia, an app not only needs to be signed and notarized, but a shell script no longer works as a launcher. Tested: blubblaunch compiled with JDK 21 results in a binary and applications that work with JRE 11 (Adoptium), JRE 17 (Adoptium), JRE 21 (Adoptium).

# How to use
* Download a JDK that matches your CPU architecture
* Install XCode including command line tools
* Check out this repo (or really, just copy the code)
* Compile the launcher:
`clang -arch x86_64 -arch arm64 -I$JDKPATH/include -I$JDKPATH/include/darwin -framework Cocoa -o blubblaunch blubblaunch.c`
* Build the directory structure of your mac app
* Place the blubblaunch binary in Contents/MacOS
* Edit Info.plist so that CFBundleExecutable is set to blubblaunch
* Put your main jar into Contents/Java
* If you have supplemental libraries, you need to put them into the app bundle as well and your main jar must have a MANIFEST.MF that lists them as relative paths from the main jar.
* I recommend you use the script from https://incenp.org/notes/2023/universal-java-app-on-macos.html to create a universal JRE
* Put your JRE somewhere into Contents/
* Create the text file Contents/blubblaunch.config with exactly three lines: your main class, the path to your main jar and the path to the JRE Home. For example:
```
org.myproject.ApplicationMain
Java/myproject.jar
runtime/adoptopenjdk.jre/Contents/Home
```
* At this point, you can call blubblaunch from the command line - it should start the application, or at least print some helpful messages
* Sign (out of scope for this document. Note: for Sequoia, you need to have entitlements)
* Notarize (out of scope for this document)

# Example
Feel free to download the Mac app from https://datendestille.de and look at its internals. The applications are German-language only, but for understanding the startup process you'll be fine.

# Limitations
* VM options, e.g. -Xmx for memory allocation, are hard-coded into the launcher.
* The configuration file format is rudimentary and inflexible.
* JRE 7 (from Oracle) kind of works - however, there is no ARM version for that ancient software. So I compiled blubblaunch as Intel-only, that triggers Rosetta emulation and the software starts in principle. However, it is not possible to notarize the resulting software, because JRE 7 links to a version of the standard lib that is too old for Apple's current policies. So users have to open Systems Settings and allow starting the software.
* The tool `jpackage`, included in every JDK, is an attractive alternative if you do not need a universal application. It also includes a launcher, but I did not manage to isolate it enough to use in my own applications.

# Acknowledgements
The launcher is based on the code by Damien Goutte-Gattat (who has allowed me to share it under BSD-2 license) found on https://incenp.org/notes/2023/universal-java-app-on-macos.html
