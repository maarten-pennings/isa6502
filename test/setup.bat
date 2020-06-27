@ECHO OFF
ECHO Setting up standalone virtual python environment
python.exe -m venv env
CALL env\Scripts\activate.bat
python -m pip install --upgrade pip setuptools wheel
IF EXIST requirements.txt (
   pip install -r requirements.txt
)
