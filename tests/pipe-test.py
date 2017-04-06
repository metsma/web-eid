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
import subprocess
import struct
import sys
import unittest
import re
import testconf
from chrome import ChromeTest

# The protocol datagram is described here:
# https://developer.chrome.com/extensions/nativeMessaging#native-messaging-host-protocol

version_re = "^(\d+|\.)+$"
class TestHostPipe(ChromeTest):

  def test_random_string(self):
      cmd = "BLAH"
      resp = self.transceive_dumb(cmd)
      self.assertEquals(resp["error"], "protocol")
      self.assertTrue(re.compile(version_re).match(resp["version"]))
      self.assertEqual(self.p.wait(), 1)

  def test_plain_string(self):
      self.p.stdin.write("Hello World!")
      resp = self.get_response()
      self.assertEquals(resp["error"], "protocol")
      self.assertTrue(re.compile(version_re).match(resp["version"]))
      self.assertEqual(self.p.wait(), 1)

  def test_empty_json(self):
      cmd = {}
      resp = self.transceive_dumb(json.dumps(cmd))
      self.assertEquals(resp["error"], "protocol")
      self.assertTrue(re.compile(version_re).match(resp["version"]))
      self.assertEqual(self.p.wait(), 1)

  def test_utopic_length(self):
      # write big bumber and little data
      self.p.stdin.write(struct.pack("=I", 0xFFFFFFFF))
      self.p.stdin.write("Hello World!")
      resp = self.get_response()
      self.assertEquals(resp["error"], "protocol")
      self.assertEqual(self.p.wait(), 1)

#  def test_length_exceeds_data(self):
#      # write length > data size
#      self.p.stdin.write(struct.pack("=I", 0x0000000F))
#      self.p.stdin.write("Hello World!")
#      resp = self.get_response()
#      self.assertEquals(resp["result"], "invalid_argument")
#      self.assertEqual(self.p.wait(), 1)

if __name__ == '__main__':
    # run tests
    unittest.main()
