using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class SetNicknamePanel : MonoBehaviour
{
    private TMP_InputField _input;

    private void Awake()
    {
        _input = transform.GetChild(1)?.GetComponent<TMP_InputField>();

        if( _input == null )
        {
            Debug.Log($"SetNicknamePanel::Awake : input null ref.");
        }
    }

    public async void SetName()
    {
        if (_input == null)
        {
            Debug.Log($"SetNicknamePanel::Awake : input null ref.");
        }

        UserData.Instance.SetName(_input.text);
        await PacketMaker.Instance.ReqSetNickname(_input.text);

        return;
    }
}
