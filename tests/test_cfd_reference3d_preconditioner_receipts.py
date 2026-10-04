"""Apply the existing immutable supervision contract to the preconditioner lane."""
import sys
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parent))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import test_cfd_reference3d_method_receipts as contract
import run_cfd_reference3d_preconditioner as supervisor


class PreconditionerReceipts(contract.ReceiptContract):
    def exercise(self,*args,**kwargs):
        with patch.object(contract,'runner',supervisor):
            return super().exercise(*args,**kwargs)


if __name__=='__main__':unittest.main()
