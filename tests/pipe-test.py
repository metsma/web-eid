# Copyright (C) 2017 Martin Paljak

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
