from werkzeug.exceptions import HTTPException
from cobra_system_control.exceptions import FPGAFileError


class FPGAUpdateError(HTTPException, FPGAFileError):
    status_code = 555
    description = "FPGA is updating don't power down the system!"
