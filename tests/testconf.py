# Copyright (C) 2017 Martin Paljak

import sys
import os.path
import os

def get_exe():
    if "EXE" in os.environ:
        return os.environ["EXE"]
    if sys.platform == 'darwin':
        os.environ["WEB_EID_APP"] = "src/Web eID.app/Contents/MacOS/Web eID"
        return "src/nm-bridge/web-eid-bridge"
    elif sys.platform == "linux2":
        os.environ["WEB_EID_APP"] = "src/web-eid"
        return "src/nm-bridge/web-eid-bridge"
    elif sys.platform == 'win32':
        if os.path.isfile("src\\debug\\web-eid-bridge.exe"):
            os.environ["WEB_EID_APP"] = "src/debug/Web-eID.exe"
            return "src\\debug\\web-eid-bridge.exe"
        else:
            os.environ["WEB_EID_APP"] = "src/release/Web-eID.exe"
            return "src\\release\\web-eid-bridge.exe"
    else:
        print("Unsupported platform: %s" % sys.platform)
        sys.exit(1)

                                                        
