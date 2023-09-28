BASE_TEXT = """
format Step%s:
    name: "step%s"
    value :u64
    step  :u%s

"""
import sys
import textwrap


def gen_large(count):
    for i in range(1, count):
        print(BASE_TEXT % (i, i, i))


def gen_indent(count):
    indent = "    "
    print(BASE_TEXT % (1, 1, 1))
    for i in range(2, count):
        print(textwrap.indent(BASE_TEXT % (i, i, i), indent))
        indent += "    "


if __name__ == "__main__":
    count = int(sys.argv[2])
    mode = sys.argv[1]
    match mode:
        case "flat":
            gen_large(count)
        case "nest":
            gen_indent(count)
        case _:
            raise ValueError("unknown mode")
