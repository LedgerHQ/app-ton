import pytest

from application_client.ton_command_sender import BoilerplateCommandSender, Errors
from application_client.ton_response_unpacker import unpack_sign_data_response
from application_client.ton_sign_data import PlaintextSignDataRequest, SignDataRequest, AppDataSignDataRequest, PlaintextSignDataNewRequest, BinarySignDataNewRequest, CellSignDataNewRequest, SignDataNewRequest
from ragger.error import ExceptionRAPDU
from ragger.navigator import NavInsID, NavIns
from ledgered.devices import DeviceType
from utils import ROOT_SCREENSHOT_PATH, check_signature_validity
from typing import List
from tonsdk.boc import Cell
from tonsdk.utils import Address
from tonsdk.contract.wallet import WalletV4ContractR2


def test_sign_data(backend, navigator, test_name):
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    # The path used for this entire test
    path: str = "m/44'/607'/0'/0'/0'/0'"

    # First we need to get the public key of the device in order to build the transaction
    pubkey = client.get_public_key(path=path).data

    requests: List[SignDataRequest] = [
        PlaintextSignDataRequest("a" * 120),
        AppDataSignDataRequest(Cell(), address=Address("0:" + "0" * 64), domain="test.ton", ext=Cell())
    ]

    # Enable blind signing and expert mode
    if backend.device.is_nano:
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                        test_name + "/pretest",
                                        [
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                        ],
                                        screen_change_before_first_instruction=False)
    else:
        if backend.device.type == DeviceType.APEX_P:
            touch_pos_1 = (265, 95)
            touch_pos_2 = (265, 215)
        else:
            touch_pos_1 = (354, 125)
            touch_pos_2 = (354, 272)
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                        test_name + "/pretest",
                                        [
                                            NavInsID.USE_CASE_HOME_INFO,
                                            NavIns(NavInsID.TOUCH, touch_pos_1),
                                            NavIns(NavInsID.TOUCH, touch_pos_2),
                                            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
                                        ],
                                        screen_change_before_first_instruction=False)

    for (i, request) in enumerate(requests):
        # Send the sign device instruction.
        # As it requires on-screen validation, the function is asynchronous.
        # It will yield the result when the navigation is done
        with client.sign_data(path=path, data=request.to_request_bytes()):
            # Validate the on-screen request by performing the navigation appropriate for this device
            if backend.device.is_nano:
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                            [NavInsID.BOTH_CLICK],
                                                            "Approve",
                                                            ROOT_SCREENSHOT_PATH,
                                                            test_name + f"/part{i}")
            else:
                instructions = [
                    NavInsID.SWIPE_CENTER_TO_LEFT,
                ]
                if type(request) != PlaintextSignDataRequest:
                    pre = [
                        NavInsID.USE_CASE_CHOICE_REJECT,
                        NavInsID.USE_CASE_CHOICE_CONFIRM,
                    ]
                    instructions = pre + instructions
                navigator.navigate(instructions)
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_VIEW_DETAILS_NEXT,
                                                            [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                            NavInsID.USE_CASE_STATUS_DISMISS],
                                                            "Hold to sign",
                                                            ROOT_SCREENSHOT_PATH,
                                                            test_name + f"/part{i}",
                                                            screen_change_before_first_instruction=False)

        # The device as yielded the result, parse it and ensure that the signature is correct
        response = client.get_async_response().data
        sig, hash_b = unpack_sign_data_response(response)
        assert hash_b == request.to_cell().bytes_hash()
        assert check_signature_validity(pubkey, sig, request.to_signed_data())


def test_sign_data_refused(backend, navigator, test_name):
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    path: str = "m/44'/607'/0'/0'/0'/0'"

    request = PlaintextSignDataRequest("test")
    rb = request.to_request_bytes()

    if backend.device.is_nano:
        with pytest.raises(ExceptionRAPDU) as e:
            with client.sign_data(path=path, data=rb):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Reject",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

        # Assert that we have received a refusal
        assert e.value.status == Errors.SW_DENY
        assert len(e.value.data) == 0
    else:
        for i in range(3):
            instructions = []
            if i > 0:
                instructions += [NavInsID.SWIPE_CENTER_TO_LEFT]
                instructions += [NavInsID.USE_CASE_VIEW_DETAILS_NEXT] * (i-1)
            instructions += [NavInsID.USE_CASE_REVIEW_REJECT,
                             NavInsID.USE_CASE_CHOICE_CONFIRM,
                             NavInsID.USE_CASE_STATUS_DISMISS]
            with pytest.raises(ExceptionRAPDU) as e:
                with client.sign_data(path=path, data=rb):
                    navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                                   test_name + f"/part{i}",
                                                   instructions)
            # Assert that we have received a refusal
            assert e.value.status == Errors.SW_DENY
            assert len(e.value.data) == 0


def test_sign_data_new(backend, navigator, test_name):
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    # The path used for this entire test
    path: str = "m/44'/607'/0'/0'/0'/0'"

    # First we need to get the public key of the device in order to build the transaction
    pubkey = client.get_public_key(path=path).data

    # private_key does not participate in address calculation, which we need; but it is required for object initialization
    wallet = WalletV4ContractR2(public_key=pubkey, private_key=pubkey)
    expected_address = wallet.address

    requests: List[SignDataNewRequest] = [
        PlaintextSignDataNewRequest("a" * 120, "test.ton"),
        BinarySignDataNewRequest(b"a" * 120, "test.ton"),
        CellSignDataNewRequest(Cell(), 0x12345678, "test.ton"),
        PlaintextSignDataNewRequest("a" * 120, "test." * 24 + "ab.ton"), # app domain length = 126
    ]

    # Enable blind signing and expert mode
    if backend.device.is_nano:
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                        test_name + "/pretest",
                                        [
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                            NavInsID.RIGHT_CLICK,
                                            NavInsID.BOTH_CLICK,
                                        ],
                                        screen_change_before_first_instruction=False)
    else:
        if backend.device.type == DeviceType.APEX_P:
            touch_pos_1 = (265, 95)
            touch_pos_2 = (265, 215)
        else:
            touch_pos_1 = (354, 125)
            touch_pos_2 = (354, 272)
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                        test_name + "/pretest",
                                        [
                                            NavInsID.USE_CASE_HOME_INFO,
                                            NavIns(NavInsID.TOUCH, touch_pos_1),
                                            NavIns(NavInsID.TOUCH, touch_pos_2),
                                            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
                                        ],
                                        screen_change_before_first_instruction=False)

    for (i, request) in enumerate(requests):
        # Send the sign device instruction.
        # As it requires on-screen validation, the function is asynchronous.
        # It will yield the result when the navigation is done
        with client.sign_data(path=path, data=request.to_request_bytes(), new_format=True):
            # Validate the on-screen request by performing the navigation appropriate for this device
            if backend.device.is_nano:
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                            [NavInsID.BOTH_CLICK],
                                                            "Approve",
                                                            ROOT_SCREENSHOT_PATH,
                                                            test_name + f"/part{i}")
            else:
                instructions = [
                    NavInsID.SWIPE_CENTER_TO_LEFT,
                ]
                if type(request) != PlaintextSignDataNewRequest:
                    pre = [
                        NavInsID.USE_CASE_CHOICE_REJECT,
                        NavInsID.USE_CASE_CHOICE_CONFIRM,
                    ]
                    instructions = pre + instructions
                navigator.navigate(instructions)
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_VIEW_DETAILS_NEXT,
                                                            [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                            NavInsID.USE_CASE_STATUS_DISMISS],
                                                            "Hold to sign",
                                                            ROOT_SCREENSHOT_PATH,
                                                            test_name + f"/part{i}",
                                                            screen_change_before_first_instruction=False)

        # The device as yielded the result, parse it and ensure that the signature is correct
        response = client.get_async_response().data
        sig, hash_b = unpack_sign_data_response(response)
        assert hash_b == request.to_signed_data(expected_address)
        assert check_signature_validity(pubkey, sig, hash_b)


def test_sign_data_new_refused(backend, navigator, test_name):
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    path: str = "m/44'/607'/0'/0'/0'/0'"

    request = PlaintextSignDataNewRequest("test", "test.ton")
    rb = request.to_request_bytes()

    if backend.device.is_nano:
        with pytest.raises(ExceptionRAPDU) as e:
            with client.sign_data(path=path, data=rb, new_format=True):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Reject",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

        # Assert that we have received a refusal
        assert e.value.status == Errors.SW_DENY
        assert len(e.value.data) == 0
    else:
        for i in range(3):
            instructions = []
            if i > 0:
                instructions += [NavInsID.SWIPE_CENTER_TO_LEFT]
                instructions += [NavInsID.USE_CASE_VIEW_DETAILS_NEXT] * (i-1)
            instructions += [NavInsID.USE_CASE_REVIEW_REJECT,
                             NavInsID.USE_CASE_CHOICE_CONFIRM,
                             NavInsID.USE_CASE_STATUS_DISMISS]
            with pytest.raises(ExceptionRAPDU) as e:
                with client.sign_data(path=path, data=rb, new_format=True):
                    navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                                   test_name + f"/part{i}",
                                                   instructions)
            # Assert that we have received a refusal
            assert e.value.status == Errors.SW_DENY
            assert len(e.value.data) == 0
