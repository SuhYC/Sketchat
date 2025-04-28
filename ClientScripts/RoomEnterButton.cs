using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;


public class RoomEnterButton : MonoBehaviour
{
    private TMPro.TMP_InputField inputfield;
    

    private void Awake()
    {
        inputfield = transform.parent.GetChild(0)?.GetComponent<TMPro.TMP_InputField>();

        if(inputfield == null )
        {
            Debug.Log($"RoomEnterButton::Awake : null ref");
        }
    }


    public async void OnClick()
    {
        string pw = inputfield.text;

        ushort selectedRoomNum = RoomPanel.Instance.GetSelectedRoomNum();

        await PacketMaker.Instance.ReqEnterRoom(selectedRoomNum, pw);
    }
}
