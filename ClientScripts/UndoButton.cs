using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class UndoButton : MonoBehaviour
{
    
    public async void OnClick()
    {
        await PacketMaker.Instance.ReqUndo();
    }
}
