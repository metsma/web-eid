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
import json

from chrome import ChromeTest

def b64pad(s):
   return s + '=' * (4 - len(s) % 4)


class TestAuth(ChromeTest):
  def test_simple_auth(self):
      nonce = str(uuid.uuid4())
      cmd = {"auth": {"nonce": nonce}}
      resp = self.transact(cmd)
      self.assertEqual("token" in resp, True)
      # basic verification of returned token
      jwt = str(resp["token"])
      c = jwt.split(".");
      header = json.loads(base64.urlsafe_b64decode(b64pad(c[0])))
      payload = json.loads(base64.urlsafe_b64decode(b64pad(c[1])))
      self.assertEquals(payload["aud"], "https://example.com")
      self.assertEquals(payload["nonce"], nonce)
      print json.dumps(header, indent = 4)
      print json.dumps(payload, indent = 4)

if __name__ == '__main__':
    # run tests
    unittest.main()
