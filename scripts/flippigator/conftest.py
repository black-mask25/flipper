import asyncio
import logging
import os
import sys
import time

import allure
import pytest
import serial
from flippigator.extensions.proxmark_wrapper import proxmark_wrapper
from flippigator.flippigator import (
    FlippigatorException,
    Gator,
    Navigator,
    Reader,
    Relay,
)
from async_protopy.connectors.serial_connector import SerialConnector
from async_protopy.clients.protobuf_client import FlipperProtobufClient


os.system("color")


@pytest.fixture(scope="session")
def event_loop():
    loop = asyncio.get_event_loop()
    yield loop
    loop.close()


def is_windows():
    # check for windows
    return sys.platform.startswith("win")


def pytest_addoption(parser):
    # here you can pass any arguments you want
    """
    You can add static name for usb devices by adding rules to /etc/udev/rules.d/99-usb-serial.rules
    SUBSYSTEM=="tty", ATTRS{idVendor}=="%your_device_VID%", ATTRS{idProduct}=="%your_device_PID%",
    ATTRS{serial}=="%your_device_serial%", SYMLINK+="name"
    pytest -v --scale=8 --bench_nfc_rfid --bench_ibutton_ir --port=/dev/bench_flipper --reader_port=/dev/reader_arduino --flipper_r_port=/dev/ireader_flipper --flipper_k_port=/dev/ikey_flipper --relay_port=/dev/relay_arduino
    """
    parser.addoption("--port", action="store", default=None, help="flipper serial port")
    parser.addoption(
        "--bench_port", action="store", default=None, help="bench serial port"
    )
    parser.addoption(
        "--reader_port", action="store", default=None, help="reader serial port"
    )
    parser.addoption(
        "--relay_port",
        action="store",
        default=None,
        help="relay controller serial port",
    )
    parser.addoption(
        "--flipper_r_port",
        action="store",
        default=None,
        help="flipper ibutton reader and IR serial port",
    )
    parser.addoption(
        "--flipper_k_port",
        action="store",
        default=None,
        help="flipper ibutton key and IR serial port",
    )
    parser.addoption(
        "--path", action="store", default="./img/ref/", help="path to reference images"
    )
    parser.addoption("--debugger", action="store", default=True, help="debug flag")
    parser.addoption("--gui", action="store", default=True, help="gui flag")
    parser.addoption("--scale", action="store", default=8, help="scale factor")
    parser.addoption("--threshold", action="store", default=0.99, help="threshold")
    parser.addoption(
        "--bench_nfc_rfid",
        action="store_true",
        default=False,
        help="use this flag for E2E rfid and nfc bench tests",
    )
    parser.addoption(
        "--bench_ibutton_ir",
        action="store_true",
        default=False,
        help="use this flag for E2E ibutton and ir bench tests",
    )
    parser.addoption(
        "--no_init",
        action="store_true",
        default=False,
        help="use this flag for skip setup cycle",
    )
    parser.addoption(
        "--px3_path",
        action="store",
        default="C:\\Users\\User\\Desktop\\proxmark3\\client\\proxmark3.exe",
        help="path to proxmark3 executable file",
        type=str,
    )
    parser.addoption(
        "--update_flippers",
        action="store_true",
        default=False,
        help="use this flag for update all flippers",
    )


def pytest_configure(config):
    # here you can add setup before launching session!
    pass


def pytest_unconfigure(config):
    # here you can add teardown after session!
    pass


def pytest_collection_modifyitems(config, items):
    if config.getoption("--debugger"):
        logging.basicConfig(level=logging.DEBUG)
    if config.getoption("--bench_nfc_rfid"):
        logging.info("NFC and RFID bench option selected")
    else:
        logging.info("NFC and RFID bench option unselected")
        skip_bench_nfc_rfid = pytest.mark.skip(
            reason="need --bench_nfc_rfid option to run"
        )

        for item in items:
            if "bench_nfc_rfid" in item.keywords:
                item.add_marker(skip_bench_nfc_rfid)
    if config.getoption("--bench_ibutton_ir"):
        logging.info("IButton and Infrared bench option selected")
    else:
        logging.info("IButton and Infrared bench option unselected")
        skip_bench_ibutton_ir = pytest.mark.skip(
            reason="need --bench__ibutton_ir option to run"
        )

        for item in items:
            if "bench_ibutton_ir" in item.keywords:
                item.add_marker(skip_bench_ibutton_ir)


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    report = outcome.get_result()
    if report.when == "call" and report.failed:
        with allure.step("Failure screenshot"):
            logging.warning("Recognition failure, screenshot will be saved")
            nav = item.funcargs.get("nav")

            if nav:
                nav.save_screen("failed.bmp")
                allure.attach.file(
                    "img/failed.bmp",
                    name="last_screenshot",
                    attachment_type=allure.attachment_type.BMP,
                )


@pytest.fixture(scope="session")
def flipper_serial(request):
    # taking port from config or returning OS based default
    port = request.config.getoption("--port")
    if port:
        pass
    elif is_windows():
        port = "COM4"
    else:
        port = "/dev/ttyACM0"

    try:

        flipper_serial = SerialConnector(url=port, baud_rate=230400, timeout=5)
    except serial.serialutil.SerialException:  # TODO exception
        logging.error("can not open serial port")
        sys.exit(0)

    logging.debug("Flipper serial port opened on" + port)
    return flipper_serial


@pytest.fixture(scope="session")
async def bench_serial(request):
    # taking port from config or returning OS based default
    port = request.config.getoption("--bench_port")
    if port:
        pass
    elif is_windows():
        port = "COM5"
    else:
        port = "/dev/ttyUSB0"

    bench_serial = SerialConnector(url=port, baud_rate=115200, timeout=1)

    await asyncio.sleep(3)

    await bench_serial.drain()
    await bench_serial.clear_read_buffer()

    logging.debug("NFC and RFID bench serial port opened on" + port)
    return bench_serial


@pytest.fixture(scope="session")
async def reader_serial(request):
    # taking port from config or returning OS based default
    port = request.config.getoption("--reader_port")
    if port:
        pass
    elif is_windows():
        port = "COM6"
    else:
        port = "/dev/ttyACM1"

    reader_serial = SerialConnector(url=port, baud_rate=115200, timeout=1)

    await asyncio.sleep(3)

    await bench_serial.drain()
    await bench_serial.clear_read_buffer()

    logging.debug("NFC and RFID Readers serial port opened on" + port)
    return reader_serial


@pytest.fixture(scope="session")
async def flipper_reader_serial(request):
    # taking port from config or returning OS based default
    port = request.config.getoption("--flipper_r_port")
    if port:
        pass
    elif is_windows():
        port = "COM7"
    else:
        port = "/dev/ttyACM2"

    flipper_reader_serial = SerialConnector(url=port, baud_rate=2304000, timeout=1)

    await asyncio.sleep(3)

    await bench_serial.drain()
    await bench_serial.clear_read_buffer()

    logging.debug("Flipper 'reader' serial port opened on" + port)
    return flipper_reader_serial


@pytest.fixture(scope="session")
async def flipper_key_serial(request):
    # taking port from config or returning OS based default
    port = request.config.getoption("--flipper_k_port")
    if port:
        pass
    elif is_windows():
        port = "COM8"
    else:
        port = "/dev/ttyACM3"

    flipper_key_serial = SerialConnector(url=port, baud_rate=2304000, timeout=1)

    await asyncio.sleep(3)

    await bench_serial.drain()
    await bench_serial.clear_read_buffer()

    logging.debug("Flipper 'key' serial port opened on" + port)
    return flipper_key_serial


@pytest.fixture(scope="session")
async def relay_serial(request):
    # taking port from config or returning OS based default
    port = request.config.getoption("--relay_port")
    if port:
        pass
    elif is_windows():
        port = "COM9"
    else:
        port = "/dev/ttyACM4"

    relay_serial = SerialConnector(url=port, baud_rate=115200, timeout=1)

    await asyncio.sleep(3)

    await bench_serial.drain()
    await bench_serial.clear_read_buffer()

    logging.debug("Relay serial port opened on" + port)
    return relay_serial


@pytest.fixture(scope="session")
async def nav(flipper_serial, request):
    if request.config.getoption("--update_flippers"):
        port = request.config.getoption("--port")
        if port:
            pass
        elif is_windows():
            port = "COM4"
        else:
            port = "/dev/ttyACM0"

        os.system("ufbt dotenv_create")
        os.system("ufbt update --channel=dev")
        os.system("ufbt flash_usb FLIP_PORT=" + port)
        await asyncio.sleep(2)

        start_time = time.time()
        while time.time() - start_time < 120:
            try:
                flipper_serial = SerialConnector(url=port, baud_rate=230400, timeout=1)
            except serial.serialutil.SerialException:
                await asyncio.sleep(1)
                print(
                    "Waiting for flipper boot after update "
                    + str(int(time.time() - start_time))
                    + "s"
                )
                logging.debug(
                    "Waiting for flipper boot after update "
                    + str(int(time.time() - start_time))
                    + "s"
                )
                if time.time() - start_time > 120:
                    logging.error("can not open serial port")
                    sys.exit(0)
                    break
            else:
                break

        await bench_serial.drain()
        await bench_serial.clear_read_buffer()
        # flipper_serial.timeout = 5 # TODO change serial timeout
        logging.debug("Flipper serial port opened on" + port)

    proto = FlipperProtobufClient(flipper_serial)
    await proto.start()
    logging.debug("RPC session of main flipper started")

    path = request.config.getoption("--path")
    gui = request.config.getoption("--gui")
    debug = request.config.getoption("--debugger")
    scale = request.config.getoption("--scale")
    threshold = request.config.getoption("--threshold")

    nav = Navigator(
        proto,
        debug=debug,
        gui=gui,
        scale=int(scale),
        threshold=threshold,
        path=path,
        window_name="Main flipper",
    )
    await nav.update_screen()

    if not (request.config.getoption("--no_init")):
        # Enabling of bluetooth
        await nav.go_to_main_screen()
        await nav.press_ok()
        await nav.go_to("Settings")
        await nav.press_ok()
        await nav.go_to("Bluetooth")
        await nav.press_ok()
        await nav.update_screen()
        menu = await nav.get_menu_list()
        if "BluetoothIsON" in menu:
            pass
        elif "BluetoothIsOFF" in menu:
            await nav.press_right()
        else:
            raise FlippigatorException("Can not enable bluetooth")

        # Enabling Debug
        await nav.press_back()
        await nav.go_to("System")
        await nav.press_ok()
        menu = await nav.get_menu_list()
        if "DebugIsON" in menu:
            pass
        elif "DebugIsOFF" in menu:
            await nav.go_to("DebugIsOFF")
            await nav.press_right()
        else:
            raise FlippigatorException("Can not enable debug")

    yield nav
    await proto.stop()



@pytest.fixture(scope="session")
def px(request):
    px = proxmark_wrapper(request.config.getoption("--px3_path"))
    return px


@pytest.fixture(scope="session")
async def nav_reader(flipper_reader_serial, request):
    if request.config.getoption("--update_flippers"):
        port = request.config.getoption("--flipper_r_port")
        if port:
            pass
        elif is_windows():
            port = "COM7"
        else:
            port = "/dev/ttyACM2"

        os.system("ufbt dotenv_create")
        os.system("ufbt update --channel=dev")
        os.system("ufbt flash_usb FLIP_PORT=" + port)
        await asyncio.sleep(2)

        start_time = time.time()
        while time.time() - start_time < 120:
            try:
                flipper_serial = SerialConnector(url=port, baud_rate=230400, timeout=1)
            except serial.serialutil.SerialException:
                time.sleep(1)
                print(
                    "Waiting for flipper boot after update "
                    + str(int(time.time() - start_time))
                    + "s"
                )
                logging.debug(
                    "Waiting for flipper boot after update "
                    + str(int(time.time() - start_time))
                    + "s"
                )
                if time.time() - start_time > 120:
                    logging.error("can not open 'reader' serial port")
                    sys.exit(0)
                    break
            else:
                break

        await bench_serial.drain()
        await bench_serial.clear_read_buffer()
        # flipper_serial.timeout = 5  # TODO: change serial timeout
        logging.debug("Flipper 'reader' serial port opened on" + port)

    proto = FlipperProtobufClient(flipper_reader_serial)
    await proto.start()
    logging.debug("RPC session of flipper 'reader' started")

    path = request.config.getoption("--path")
    gui = request.config.getoption("--gui")
    debug = request.config.getoption("--debugger")
    scale = request.config.getoption("--scale")
    threshold = request.config.getoption("--threshold")

    nav_reader = Navigator(
        proto,
        debug=debug,
        gui=gui,
        scale=int(scale),
        threshold=threshold,
        path=path,
        window_name="Reader flipper",
    )
    await nav_reader.update_screen()
    if not (request.config.getoption("--no_init")):
        # Enabling of bluetooth
        await nav_reader.go_to_main_screen()
        await nav_reader.press_ok()
        await nav_reader.go_to("Settings")
        await nav_reader.press_ok()
        await nav_reader.go_to("Bluetooth")
        await nav_reader.press_ok()
        await nav_reader.update_screen()
        menu = await nav_reader.get_menu_list()
        if "BluetoothIsON" in menu:
            pass
        elif "BluetoothIsOFF" in menu:
            nav_reader.press_right()
        else:
            raise FlippigatorException("Can not enable bluetooth")

        # Enabling Debug
        await nav_reader.press_back()
        await nav_reader.go_to("System")
        await nav_reader.press_ok()
        menu = await nav_reader.get_menu_list()
        if "DebugIsON" in menu:
            pass
        elif "DebugIsOFF" in menu:
            await nav_reader.go_to("DebugIsOFF")
            await nav_reader.press_right()
        else:
            raise FlippigatorException("Can not enable debug")

    yield nav_reader
    await proto.start()


@pytest.fixture(scope="session")
async def nav_key(flipper_key_serial, request):
    if request.config.getoption("--update_flippers"):
        port = request.config.getoption("--flipper_k_port")
        if port:
            pass
        elif is_windows():
            port = "COM8"
        else:
            port = "/dev/ttyACM3"

        os.system("ufbt dotenv_create")
        os.system("ufbt update --channel=dev")
        os.system("ufbt flash_usb FLIP_PORT=" + port)
        await asyncio.sleep(2)

        start_time = time.time()
        while time.time() - start_time < 120:
            try:
                flipper_serial = SerialConnector(url=port, baud_rate=230400, timeout=1)
            except serial.serialutil.SerialException:
                await asyncio.sleep(1)
                print(
                    "Waiting for flipper boot after update "
                    + str(int(time.time() - start_time))
                    + "s"
                )
                logging.debug(
                    "Waiting for flipper boot after update "
                    + str(int(time.time() - start_time))
                    + "s"
                )
                if time.time() - start_time > 120:
                    logging.error("can not open 'key' serial port")
                    sys.exit(0)
                    break
            else:
                break

        await bench_serial.drain()
        await bench_serial.clear_read_buffer()
        # flipper_serial.timeout = 5 # TODO: change serial timeout
        logging.debug("Flipper 'key' serial port opened on" + port)

    proto = FlipperProtobufClient(flipper_key_serial)
    await proto.start()
    logging.debug("RPC session of flipper 'key' started")

    path = request.config.getoption("--path")
    gui = request.config.getoption("--gui")
    debug = request.config.getoption("--debugger")
    scale = request.config.getoption("--scale")
    threshold = request.config.getoption("--threshold")

    nav_key = Navigator(
        proto,
        debug=debug,
        gui=gui,
        scale=int(scale),
        threshold=threshold,
        path=path,
        window_name="Key flipper",
    )
    await nav_key.update_screen()
    if not (request.config.getoption("--no_init")):
        # Enabling of bluetooth
        await nav_key.go_to_main_screen()
        await nav_key.press_ok()
        await nav_key.go_to("Settings")
        await nav_key.press_ok()
        await nav_key.go_to("Bluetooth")
        await nav_key.press_ok()
        await nav_key.update_screen()
        menu = await nav_key.get_menu_list()
        if "BluetoothIsON" in menu:
            pass
        elif "BluetoothIsOFF" in menu:
            await nav_key.press_right()
        else:
            raise FlippigatorException("Can not enable bluetooth")

        # Enabling Debug
        await nav_key.press_back()
        await nav_key.go_to("System")
        await nav_key.press_ok()
        menu = await nav_key.get_menu_list()
        if "DebugIsON" in menu:
            pass
        elif "DebugIsOFF" in menu:
            await nav_key.go_to("DebugIsOFF")
            await nav_key.press_right()
        else:
            raise FlippigatorException("Can not enable debug")

    yield nav_key
    await proto.start()


@pytest.fixture(scope="session", autouse=False)
async def gator(bench_serial, request) -> Gator:
    bench = request.config.getoption("--bench_nfc_rfid")
    if bench:
        logging.debug("Gator initialization")

        gator = Gator(bench_serial, 900, 900)
        await gator.home()

        logging.debug("Gator initialization complete")
        return gator


@pytest.fixture(scope="session", autouse=False)
def reader_nfc(reader_serial, gator, request) -> Reader:
    bench = request.config.getoption("--bench_nfc_rfid")
    if bench:
        logging.debug("NFC reader initialization")

        reader = Reader(reader_serial, gator, -925.0, -890.0)

        logging.debug("NFC reader initialization complete")
        return reader


@pytest.fixture(scope="session", autouse=False)
def reader_em_hid(reader_serial, gator, request) -> Reader:
    bench = request.config.getoption("--bench_nfc_rfid")
    if bench:
        logging.debug("Reader RFID EM and HID initialization")

        reader = Reader(reader_serial, gator, -665.0, -875.0)

        logging.debug("Reader RFID EM and HID initialization complete")
        return reader


@pytest.fixture(scope="session", autouse=False)
def reader_indala(reader_serial, gator, request) -> Reader:
    bench = request.config.getoption("--bench_nfc_rfid")
    if bench:
        logging.debug("Reader RFID Indala initialization")

        reader = Reader(reader_serial, gator, -925.0, -635.0)

        logging.debug("Reader RFID Indala initialization complete")
        return reader


@pytest.fixture(scope="session", autouse=False)
def relay(relay_serial, gator, request) -> Relay:
    bench = request.config.getoption("--bench_ibutton_ir")
    if bench:
        logging.debug("Relay module initialization")

        relay = Relay(relay_serial)

        logging.debug("Relay module initialization complete")
        return relay
