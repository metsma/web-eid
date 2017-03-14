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
import uuid
import base64
import binascii
from chrome import ChromeTest

class TestSigning(ChromeTest):

  def test_simple_sign_session(self):
      id = str(uuid.uuid4())
      cmd = {"type": "CERT", "id": id, "origin": "https://example.com/"}
      resp = self.transceive(cmd)
      self.assertEqual(resp["result"], "ok")
      self.assertEqual(resp["id"], id)
      self.assertEqual("cert" in resp, True)
      base64.b64decode(resp["cert"]) # just to make sure it parses
      cmd = {"type": "SIGN", "origin": "https://example.com/", "cert": resp["cert"], "hash": base64.b64encode(binascii.unhexlify("0001020304050607080900010203040506070809"))}
      resp = self.transceive(cmd)


if __name__ == '__main__':
    # run tests
    unittest.main()
