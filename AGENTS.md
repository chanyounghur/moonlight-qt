# Windows delivery preference

The user wants future Windows builds delivered as an installer package (MSI or
Setup), rather than requiring ZIP extraction and manual replacement of installed
files. The v4 ZIP was manually installed as a one-time exception.

Preserve the existing Windows account's Moonlight settings and host pairing.
Include mac-keyboard.json and the user instructions in installer packaging before
WiX harvesting; adding them only to the later portable deployment folder is not
sufficient. Validate upgrading the existing official installation as well as a
clean installation, and account for installer version/product identity rules.
