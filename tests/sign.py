# Copyright (C) 2017 Martin Paljak

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
      self.assertEqual("certificate" in resp, True)
      base64.b64decode(resp["certificate"]) # just to make sure it parses
      cmd = {"sign": {"certificate": resp["certificate"], "hash": base64.b64encode(binascii.unhexlify("2CADA1A6A22AA2A9BF9093281DC6C42D46142F9CABDFA490658A84677E4AA40E")), "hashalgo": "SHA-256"}}
      resp = self.transact(cmd)


if __name__ == '__main__':
    # run tests
    unittest.main()
