#[derive(Debug)]
pub enum Error {
    Io(&'static str, std::io::Error),
    UnwrapError(String),
    LengthError(&'static str),
    SetError(&'static str),
    Nested(&'static str, Box<Error>),
    ExplicitError(&'static str),
}
impl Error {
    pub fn io_error(&self) -> Option<&std::io::Error> {
        match self {
            Error::Io(_, e) => Some(e),
            Error::Nested(_, e) => e.io_error(),
            _ => None,
        }
    }
}
impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Io(s, e) => write!(f, "{}: {}", s, e),
            Error::UnwrapError(s) => write!(f, "{}", s),
            Error::LengthError(s) => write!(f, "{}", s),
            Error::SetError(s) => write!(f, "{}", s),
            Error::Nested(s, e) => write!(f, "{}: {}", s, e),
            Error::ExplicitError(s) => write!(f, "{}", s),
        }
    }
}
impl std::error::Error for Error {}
#[derive(Default, Debug, Clone, PartialEq)]
pub struct TEST_CLASS {}
impl TEST_CLASS {
    pub fn encode<W: std::io::Write>(&self, w: &mut W) -> Result<(), Error> {
        Ok(())
    }
    pub fn decode<R: std::io::Read>(r: &mut R) -> Result<TEST_CLASS, Error> {
        let mut d = Self::default();
        d.decode_impl(r)?;
        Ok(d)
    }
    pub fn decode_exact(r: &[u8]) -> Result<TEST_CLASS, Error> {
        let mut r = std::io::Cursor::new(r);
        let result = Self::decode(&mut r)?;
        if r.position() != r.get_ref().len() as u64 {
            return Err(Error::LengthError("not all bytes consumed"));
        }
        Ok(result)
    }
    pub fn decode_impl<R: std::io::Read>(&mut self, r: &mut R) -> Result<(), Error> {
        Ok(())
    }
}
