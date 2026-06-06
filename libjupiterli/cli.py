import typer
import sys, os.path, pathlib, shutil

app = typer.Typer(add_completion=False,
                  pretty_exceptions_enable=False,  # disables Rich tracebacks
                  rich_markup_mode=None,           # disables Rich markup parsing
                  context_settings={"color": False, "help_option_names": ["-h", "--help"]}
                  )

PKG_DIR = pathlib.Path(__file__).parent

@app.command()
def info1():
    print("info will be here")

@app.command()
def info():
    print("info will be here")

def main():
    app()
