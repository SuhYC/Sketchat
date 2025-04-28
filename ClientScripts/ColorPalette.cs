using System.Collections;
using System.Collections.Generic;
using Unity.VisualScripting;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class ColorPalette : MonoBehaviour, IPointerClickHandler
{
    private RawImage m_paletteImage;
    private Image m_ColorDisplay;
    private Texture2D m_texture;

    private Color[] colors; // ���� �迭

    private int textureSize = 128;  // �ؽ�ó ũ��

    void Awake()
    {
        m_paletteImage = GetComponent<RawImage>();
        m_ColorDisplay = transform.GetChild(0)?.GetComponent<Image>();

        if (m_paletteImage == null)
        {
            Debug.Log($"Colorpalette::Awake : palette null ref.");
        }

        if(m_ColorDisplay == null)
        {
            Debug.Log($"Colorpalette::Awake : display null ref.");
        }

        colors = new Color[]
        {
            Color.red, Color.green, Color.blue, Color.yellow,
            Color.cyan, Color.magenta, Color.white, Color.black,
            new Color(0.5f, 0.5f, 0f), new Color(0.5f, 0f, 0.5f), new Color(0f, 0.5f, 0.5f),
            new Color(0.7f, 0.7f, 0f), new Color(0.7f, 0f, 0.7f), new Color(0f, 0.7f, 0.7f),
            new Color(0.5f, 0.25f, 0f), new Color(0.25f, 0.5f, 0f)
        };

        m_texture = new Texture2D(textureSize, textureSize);
        m_paletteImage.texture = m_texture;
    }

    void Start()
    {
        CreatePalette();
    }

    
    void CreatePalette()
    {
        for (int y = 0; y < 4; y++)  // 4��
        {
            for (int x = 0; x < 4; x++)  // 4��
            {
                int colorIndex = y * 4 + x;  // ���� �ε��� ���
                Color color = colors[colorIndex];

                // �� 64x64 ������ ������ ä���
                for (int j = y * textureSize / 4; j < (y + 1) * textureSize / 4; j++)
                {
                    for (int i = x * textureSize / 4; i < (x + 1) * textureSize / 4; i++)
                    {
                        m_texture.SetPixel(i, j, color);
                    }
                }
            }
        }

        m_texture.Apply();  // ������� ����
    }

    public void OnPointerClick(PointerEventData eventData)
    {
        // Ŭ���� ��ġ (RawImage �������� ��ǥ)
        Vector2 localPos;
        RectTransform rect = transform.GetComponent<RectTransform>();

        RectTransformUtility.ScreenPointToLocalPointInRectangle(rect, Input.mousePosition, null, out localPos);

        // Ŭ���� ��ġ�� �ؽ�ó ��ǥ�� ��ȯ
        int x = Mathf.FloorToInt((localPos.x + rect.rect.width / 2) / (textureSize / 4));
        int y = Mathf.FloorToInt((localPos.y + rect.rect.height / 2) / (textureSize / 4));

        // ���� üũ
        if (x >= 0 && x < 4 && y >= 0 && y < 4)
        {
            int colorIndex = y * 4 + x;  // ���õ� ���� �ε���
            Color selectedColor = colors[colorIndex];  // ���� ��������

            // ���õ� ���� ǥ��
            m_ColorDisplay.color = selectedColor;
            SketchScreen.Instance.drawColor = selectedColor;
        }
    }
}
