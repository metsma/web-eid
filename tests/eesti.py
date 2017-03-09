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

import json
import unittest
import uuid
from chrome import ChromeTest

# Tests to check how Estonian is showing
class TestEstonian(ChromeTest):
  def test_simple_auth(self):
      cmd = {"type": "AUTH", "nonce": str(uuid.uuid4()), "origin": "https://example.com/", "auth_nonce": str(uuid.uuid4()), "lang": "et"}
      resp = self.transceive(json.dumps(cmd))
      self.assertEqual(resp["result"], "ok")
      self.assertEqual("auth_token" in resp, True)

  def test_simple_sign(self):
      cmd = {"type": "CERT", "nonce": str(uuid.uuid4()), "origin": "file:///some/folder/example.com/index.html", "auth_nonce": str(uuid.uuid4()), "lang": "et"}
      resp = self.transceive(json.dumps(cmd))
      self.assertEqual(resp["result"], "ok")
      self.assertEqual("cert" in resp, True)
      cmd = {"type": "SIGN", "cert": resp["cert"], "nonce": str(uuid.uuid4()), "origin": "file:///some/folder/example.com/index.html", "hash": "AQIDBAUGBwgJAAECAwQFBgcICQA=", "lang": "et"}
      resp = self.transceive(json.dumps(cmd))

if __name__ == '__main__':
    unittest.main()
