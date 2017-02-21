#
# Chrome Token Signing Native Host
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

# This is the Makefile for Windows NMake. See GNUmakefile for OSX/Linux.

!IF !DEFINED(BUILD_NUMBER)
BUILD_NUMBER=0
!ENDIF
!include VERSION.mk
SIGN = signtool sign /v /n "$(SIGNER)" /fd SHA256 /tr http://timestamp.comodoca.com/?td=sha256 /td sha256
EXE = host-windows/Release/hwcrypto-native.exe
DISTNAME = Web-eID

$(EXE): host-windows\*.cpp host-windows\*.h
	msbuild /p:Configuration=Release;Platform=Win32 /property:MAJOR_VERSION=$(MAJOR_VERSION) /property:MINOR_VERSION=$(MINOR_VERSION) /property:RELEASE_VERSION=$(RELEASE_VERSION) /property:BUILD_NUMBER=$(BUILD_NUMBER) host-windows\host-windows.sln

pkg: $(EXE)
	IF DEFINED SIGNER ($(SIGN) $(EXE))
	"$(WIX)\bin\candle.exe" -nologo host-windows\hwcrypto-native.wxs -dVERSION=$(VERSIONEX) -dPlatform=x86
	"$(WIX)\bin\light.exe" -nologo -out $(DISTNAME)_$(VERSIONEX).x86.msi hwcrypto-native.wixobj -ext WixUIExtension -ext WixUtilExtension -dPlatform=x86
	"$(WIX)\bin\candle.exe" -nologo host-windows\hwcrypto-native.wxs -dVERSION=$(VERSIONEX) -dPlatform=x64
	"$(WIX)\bin\light.exe" -nologo -out $(DISTNAME)_$(VERSIONEX).x64.msi hwcrypto-native.wixobj -ext WixUIExtension -ext WixUtilExtension -dPlatform=x64
	IF DEFINED SIGNER ($(SIGN) $(DISTNAME)_$(VERSIONEX).x86.msi)
	IF DEFINED SIGNER ($(SIGN) $(DISTNAME)_$(VERSIONEX).x64.msi)

test: build
	python host-test\pipe-test.py -v
