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

import unittest
import time
from chrome import ChromeTest

class TestPCSC(ChromeTest):
  def test_pcsc_open(self):
      cmd = {"type": "CONNECT", "protocol": "*", "origin": "https://example.com/"}
      resp = self.transceive(cmd)
      if resp["result"] != "ok":
        cmd = {"type": "CONNECT", "protocol": "*", "origin": "https://example.com/"}
        resp = self.transceive(cmd)
      if resp["result"] != "ok":
        return
      #self.instruct("Just to see if dialog is running")
      for x in range(10):
        cmd = {"type": "APDU", "bytes": "00a4000400", "origin": "https://example.com/"}
        resp = self.transceive(cmd)
        if resp["result"] == "SCARD_E_CANCELLED":
          return
        time.sleep(1)
      cmd = {"type": "DISCONNECT", "origin": "https://example.com/"}
      resp = self.transceive(cmd)

if __name__ == '__main__':
    # run tests
    unittest.main()
