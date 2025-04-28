using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class RoomInfoPanel : MonoBehaviour
{
    private static RoomInfoPanel instance;

    TextMeshProUGUI roomNameText;
    TextMeshProUGUI pwText;

    private void Awake()
    {
        instance = this;

        roomNameText = transform.GetChild(0)?.GetComponent<TextMeshProUGUI>();
        pwText = transform.GetChild(1)?.GetChild(0)?.GetComponent<TextMeshProUGUI>();

        Init();
    }

    public static void Init()
    {
        if(instance == null)
        {
            Debug.Log($"RoomInfoPanel::Init : instance null ref.");
            return;
        }

        if (instance.roomNameText == null)
        {
            Debug.Log($"RoomInfoPanel::Init : roomNameText null ref.");
        }
        else
        {
            instance.roomNameText.text = UserData.Instance.GetRoomName();
        }

        if (instance.pwText == null)
        {
            Debug.Log($"RoomInfoPanel::Init : pwText null ref.");
        }
        else
        {
            instance.pwText.text = UserData.Instance.GetRoomPW();
        }
    }
}
