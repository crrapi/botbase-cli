def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", default=HOST)
    p.add_argument("cmd")
    p.add_argument("args", nargs="*")
    a = p.parse_args()
    bb = BB(a.host)
    if a.cmd == "click":
        for b in a.args: bb.click(b)
    elif a.cmd == "peek":
        print(bb.peek(int(a.args[0], 0), int(a.args[1], 0)).hex())
    else:
        bb.send(a.cmd + " " + " ".join(a.args))
        print(bb.recv())


if __name__ == "__main__":
    main()
