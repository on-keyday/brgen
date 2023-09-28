BASE_TEXT = """
format Step%s:
    name: "step%s"
    value :u64
    step  :u%s

"""
import sys

count = int(sys.argv[1])


def gen_large():
    for i in range(1, count):
        print(BASE_TEXT % (i, i, i))


if __name__ == "__main__":
    gen_large()
