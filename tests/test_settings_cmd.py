from application_client.ton_command_sender import BoilerplateCommandSender
from utils import ROOT_SCREENSHOT_PATH
from ragger.navigator import NavInsID, NavIns

# In this test we check the behavior of the device when asked to provide the app version
def test_app_settings(firmware, backend, navigator, test_name):
    # Use the app interface instead of raw interface
    client = BoilerplateCommandSender(backend)
    rapdu = client.get_app_settings()
    assert(len(rapdu.data) == 1 and rapdu.data[0] == 0x00)

        # Enable blind signing and expert mode
    if firmware.device.startswith("nano"):
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                        test_name,
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
        if firmware.device == "apex_p":
            touch_pos_1 = (230, 85)
            touch_pos_2 = (230, 185)
        else:
            touch_pos_1 = (354, 125)
            touch_pos_2 = (354, 272)
        
        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH,
                                        test_name,
                                        [
                                            NavInsID.USE_CASE_HOME_INFO,
                                            NavIns(NavInsID.TOUCH, touch_pos_1),
                                            NavIns(NavInsID.TOUCH, touch_pos_2),
                                            NavInsID.USE_CASE_SETTINGS_NEXT,
                                            NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
                                        ],
                                        screen_change_before_first_instruction=False)

    rapdu = client.get_app_settings()
    assert(len(rapdu.data) == 1 and rapdu.data[0] == (0x01 | 0x02))

