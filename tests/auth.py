# Copyright (C) 2017 Martin Paljak


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
      cmd = {"authenticate": {"nonce": nonce}}
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
