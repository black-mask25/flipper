import serial.tools.list_ports as list_ports


class BlackmagicResolver:
    BLACKMAGIC_HOSTNAME = "blackmagic.local"

    def __init__(self, env):
        self.list_ports = list_ports
        self.env = env

    # On Win:
    #    'location': '1-5:x.0', 'name': 'COM4',
    #    'location': '1-5:x.2', 'name': 'COM13',
    # On Linux:
    #    'location': '1-1.2:1.0', 'name': 'ttyACM0',
    #    'location': '1-1.2:1.2', 'name': 'ttyACM1',
    # On MacOS:
    #    'location': '0-1.3', 'name': 'cu.usbmodemblackmagic1',
    #    'location': '0-1.3', 'name': 'cu.usbmodemblackmagic3',
    def _find_probe(self):
        ports = list(self.list_ports.grep("blackmagic"))
        if len(ports) == 0:
            # print("Blackmagic probe serial port not found")
            pass
        elif len(ports) > 2:
            print("More than one Blackmagic probe found")
        else:
            # print("\n".join([f"{p.device} {vars(p)}" for p in ports]))
            return sorted(ports, key=lambda p: f"{p.location}_{p.name}")[0]

    # Look up blackmagic probe hostname with dns
    def _resolve_hostname(self):
        import socket

        try:
            return socket.gethostbyname(self.BLACKMAGIC_HOSTNAME)
        except socket.gaierror:
            print("Failed to resolve Blackmagic hostname")
            return None

    def get_serial(self):
        if not (probe := self._find_probe()):
            return None
        # print(f"Found Blackmagic probe on {probe.device}")
        if self.env.subst("$PLATFORM") == "win32":
            return f"\\\\.\\{probe.device}"
        return probe.device

    def get_networked(self):
        if not (probe := self._resolve_hostname()):
            return None

        return f"tcp:{probe}:2345"

    def __str__(self):
        # print("distenv blackmagic", self.env.subst("$BLACKMAGIC"))
        if (blackmagic := self.env.subst("$BLACKMAGIC")) != "auto":
            return blackmagic

        # print("Looking for Blackmagic...")
        if probe := self.get_serial() or self.get_networked():
            return probe

        raise Exception("Please specify BLACKMAGIC=...")


def generate(env):
    env.SetDefault(BLACKMAGIC_ADDR=BlackmagicResolver(env))


def exists(env):
    return True
