#
# Web eID app, (C) 2017 Web eID team and contributors
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
      cmd = {"cert": {}}
      resp = self.transact(cmd)
      self.assertEqual("cert" in resp, True)
      base64.b64decode(resp["cert"]) # just to make sure it parses
      cmd = {"sign": {"cert": resp["cert"], "hash": base64.b64encode(binascii.unhexlify("2CADA1A6A22AA2A9BF9093281DC6C42D46142F9CABDFA490658A84677E4AA40E")), "hashalgo": "SHA-256"}}
      resp = self.transact(cmd)


if __name__ == '__main__':
    # run tests
    unittest.main()
