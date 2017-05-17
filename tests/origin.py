# Copyright (C) 2017 Martin Paljak

import unittest
import uuid
from chrome import ChromeTest

# Tests to check how origin is shown
class TestAuthOrigin(ChromeTest):
  def test_simple_auth(self):
      cmd = {"type": "AUTH", "origin": "https://example.com/", "nonce": str(uuid.uuid4())}
      resp = self.transceive(cmd)
      self.assertEqual(resp["result"], "ok")
      self.assertEqual("token" in resp, True)

  def test_simple_auth_file(self):
      cmd = {"type": "AUTH", "origin": "file:///some/folder/example.com/index.html", "nonce": str(uuid.uuid4())}
      resp = self.transceive(cmd)
      self.assertEqual(resp["result"], "ok")
      self.assertEqual("token" in resp, True)

  def test_simple_auth_localhost(self):
      cmd = {"type": "AUTH", "origin": "http://localhost:8080/", "nonce": str(uuid.uuid4())}
      resp = self.transceive(cmd)
      self.assertEqual(resp["result"], "ok")
      self.assertEqual("token" in resp, True)

if __name__ == '__main__':
    unittest.main()
