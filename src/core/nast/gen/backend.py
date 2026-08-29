from .writer import Writer
from .schema import Schema

def emit_backend(w :Writer,s :Schema):
    w.line("namespace brgen::nast::backend {")
    w.line("} // namespace brgen::nast::backend")
