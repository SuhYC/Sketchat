using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class MaxUsersSlider : MonoBehaviour
{
    Slider _slider;
    TextMeshProUGUI _textMeshPro;

    private void Awake()
    {
        _slider = GetComponent<Slider>();
        if(_slider == null)
        {
            Debug.Log($"MaxUsersSlider::Awake : slider null ref.");
        }

        _textMeshPro = transform.GetChild(3)?.GetComponent<TextMeshProUGUI>();
        if(_textMeshPro == null)
        {
            Debug.Log($"MaxUsersSlider::Awake : text null ref.");
        }

    }

    public void OnChanged()
    {
        if (_slider == null)
        {
            Debug.Log($"MaxUsersSlider::Awake : slider null ref.");
            return;
        }

        if (_textMeshPro == null)
        {
            Debug.Log($"MaxUsersSlider::Awake : text null ref.");
            return;
        }

        _textMeshPro.text = _slider.value.ToString();
        return;
    }
}
