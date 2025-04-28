using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class CreateRoomButton : MonoBehaviour
{
    private TMPro.TMP_InputField _nameInputField;
    private TMPro.TMP_InputField _pwInputField;
    private Slider _slider;

    private void Awake()
    {
        _nameInputField = transform.parent.GetChild(0)?.GetComponent<TMPro.TMP_InputField>();

        if (_nameInputField == null)
        {
            Debug.Log($"CreateRoomButton::Awake : nameinput null ref.");
        }

        _pwInputField = transform.parent.GetChild(1)?.GetComponent<TMPro.TMP_InputField>();

        if( _pwInputField == null)
        {
            Debug.Log($"CreateRoomButton::Awake : pwinput null ref.");
        }

        _slider = transform.parent.GetChild(3)?.GetComponent<Slider>();
        if( _slider == null)
        {
            Debug.Log($"CreateRoomButton::Awake : maxUserSlider null ref.");
        }
    }

    public async void OnClick()
    {
        if (_nameInputField == null)
        {
            Debug.Log($"CreateRoomButton::Awake : nameinput null ref.");
            return;
        }
        string name = _nameInputField.text;

        if(_pwInputField == null)
        {
            Debug.Log($"CreateRoomButton::Awake : pwinput null ref.");
            return;
        }

        string pw = _pwInputField.text;

        if(_slider == null)
        {
            Debug.Log($"CreateRoomButton::Awake : maxUserSlider null ref.");
            return;
        }

        ushort maxuser = (ushort)_slider.value;

        await PacketMaker.Instance.ReqCreateRoom(name, pw, maxuser);
    }
}
