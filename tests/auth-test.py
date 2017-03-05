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
import re
from chrome import ChromeTest

version_re = "^(\d+|\.)+$"
class TestHostPipe(ChromeTest):
  def test_simple_auth(self):
      cmd = {"type": "AUTH", "nonce": str(uuid.uuid4()), "origin": "https://example.com/", "auth_nonce": str(uuid.uuid4())}
      resp = self.transceive(json.dumps(cmd))
      self.assertEqual(resp["result"], "ok")
      self.assertEqual("auth_token" in resp, True)

if __name__ == '__main__':
    # run tests
    unittest.main()
