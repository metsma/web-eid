# Copyright (C) 2017 Martin Paljak

import unittest
import uuid
from chrome import ChromeTest

# Tests to check how Estonian is showing
class TestEstonian(ChromeTest):
  def test_simple_auth(self):
      cmd = {"type": "AUTH", "origin": "https://example.com/", "nonce": str(uuid.uuid4()), "lang": "et"}
      resp = self.transceive(cmd)
      self.assertEqual(resp["result"], "ok")
      self.assertEqual("token" in resp, True)

  def test_simple_sign(self):
      cmd = {"type": "CERT", "origin": "file:///some/folder/example.com/index.html", "lang": "et"}
      resp = self.transceive(cmd)
      self.assertEqual(resp["result"], "ok")
      self.assertEqual("cert" in resp, True)
      cmd = {"type": "SIGN", "cert": resp["cert"], "origin": "file:///some/folder/example.com/index.html", "hash": "AQIDBAUGBwgJAAECAwQFBgcICQA=", "lang": "et"}
      resp = self.transceive(cmd)

if __name__ == '__main__':
    unittest.main()
