# Copyright (C) 2017 Martin Paljak

import unittest
import time
from chrome import ChromeTest

class TestPCSC(ChromeTest):
  def test_pcsc_open(self):
      cmd = {"SCardConnect": {"protocol": "*"}, "origin": "https://example.com:1234"}
      resp = self.transact(cmd)
      for x in range(10):
        cmd = {"SCardTransmit": {"bytes": "00a4000400"}, "origin": "https://example.com"}
        resp = self.transact(cmd)
        time.sleep(1)
      cmd = {"SCardDisconnect": {}, "origin": "https://example.com/"}
      resp = self.transact(cmd)

#  def test_pcsc_card_removal(self):
#     self.instruct("Select a reader, insert card, remove during apdu-s")
#     cmd = {"SCardConnect": {"protocol": "*"}, "origin": "https://example.com/"}
#     resp = self.transact(cmd)

if __name__ == '__main__':
    # run tests
    unittest.main()
