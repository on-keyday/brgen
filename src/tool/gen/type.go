package gen

// Type of Dart language
func (d *Type) DartString() string {
	prefix := ""
	postfix := ""
	if d.IsArray {
		prefix += "List<"
		postfix += ">"
	}
	if d.IsPtr || d.IsInterface {
		postfix += "?"
	} else if d.IsOptional {
		postfix += "?"
	}
	return prefix + d.Name + postfix
}

func (d *Type) RustString() string {
	prefix := ""
	postfix := ""
	if d.IsArray {
		prefix += "Vec<"
		postfix += ">"
	}
	if d.IsPtr {
		ptr := "Rc<"
		if d.IsWeak {
			ptr = "Weak<"
		}
		prefix += ptr + "RefCell<"
		postfix += ">" + ">"
		if !d.IsArray {
			prefix = "Option<" + prefix
			postfix = postfix + ">"
		}
	}
	if d.IsInterface && !d.IsArray {
		prefix += "Option<"
		postfix += ">"
		if d.IsWeak {
			postfix = "Weak" + postfix
		}
	}
	if d.IsOptional {
		prefix = "Option<" + prefix
		postfix = postfix + ">"
	}
	return prefix + d.Name + postfix
}

func (d *Type) TsString() string {
	postfix := ""
	if d.IsArray {
		postfix += "[]"
	} else if d.IsPtr || d.IsInterface {
		postfix += "|null"
	}
	if d.IsOptional {
		postfix += "|null"
	}
	return d.Name + postfix
}

func (d *Type) PyString() string {
	prefix := ""
	postfix := ""
	if d.IsOptional {
		prefix += "Optional["
		postfix += "]"
	}
	if d.IsArray {
		prefix += "List["
		postfix += "]"
	} else if d.IsPtr || d.IsInterface {
		prefix += "Optional["
		postfix += "]"
	}
	return prefix + d.Name + postfix
}

func (d *Type) CString() string {
	postfix := ""
	if (d.IsOptional || d.IsArray) && d.Name == "uintptr_t" {
		return "void*"
	}
	if d.IsInterface || d.IsPtr {
		postfix += "*"
	} else if !d.IsArray && d.IsOptional {
		postfix += "*"
	}
	if d.IsArray {
		postfix += "*"
	}
	return d.Name + postfix
}

func (d *Type) CSharpString() string {
	prefix := ""
	postfix := ""
	if d.IsArray {
		prefix += "List<"
		postfix += ">?"
	} else if d.IsPtr || d.IsInterface || d.IsOptional {
		postfix += "?"
	}
	return prefix + d.Name + postfix
}

func (d *Type) UintptrString() string {
	prefix := ""
	if d.IsArray {
		prefix += "[]"
	}
	if d.IsPtr || d.IsInterface {
		if !d.IsArray {
			prefix += "*"
		}
		return prefix + "uintptr"
	}
	if d.IsOptional {
		prefix += "*"
	}
	return prefix + d.Name
}
