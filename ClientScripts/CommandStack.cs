using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class CommandStack
{
    public const int MAX_STACK = 50;

    private static CommandStack _instance;

    public static CommandStack Instance
    {
        get
        {
            if(_instance == null)
            {
                _instance = new CommandStack();
            }
            return _instance;
        }
    }

    private Texture2D initTexture;
    private LinkedList<uint> DrawKeys;
    private Dictionary<uint, DrawCommand> DrawCommands;

    /// <summary>
    /// initTexture�� �ʱ�ȭ
    /// </summary>
    private CommandStack()
    {
        Init();
    }

    public void Init()
    {
        if (initTexture == null)
        {
            initTexture = new Texture2D(SketchScreen.canvasWidth, SketchScreen.canvasHeight);

            DrawKeys = new LinkedList<uint>();
            DrawCommands = new Dictionary<uint, DrawCommand>();
        }
        else
        {
            DrawKeys.Clear();
            DrawCommands.Clear();
        }

        Color[] fillColor = new Color[initTexture.width * initTexture.height];
        for (int i = 0; i < fillColor.Length; i++)
            fillColor[i] = Color.white;

        initTexture.SetPixels(fillColor);
        initTexture.Apply();
    }

    /// <summary>
    /// DrawKey�� drawNum�� userNum�� ����Ʈ������ �����Ͽ� 2����Ʈ + 2����Ʈ = 4����Ʈ�� ���� �������̴�.
    /// </summary>
    /// <param name="DrawKey"></param>
    /// <param name="vertex"></param>
    public void AddVertexToCommand(uint DrawKey, Vector2Int vertex, Color color, float width)
    {
        DrawCommand command;

        if(DrawCommands.TryGetValue(DrawKey, out command))
        {
            command.Push(vertex);

            SketchScreen.Instance.DrawLine(command.vertices[command.size - 2], vertex, width, color);
        }
        else
        {
            command = new DrawCommand(color, width);
            command.Push(vertex);
            DrawCommands.Add(DrawKey, command);

            SketchScreen.Instance.DrawAt(vertex.x, vertex.y, width, color);
        }
    }

    /// <summary>
    /// �׸������� Key,Value�� �����ϸ�
    /// Key�� Queue�� �����Ѵ�.
    /// ������ ������ Draw������ ��ġ�� DrawEnd�� ȣ���� ��� �������� �����Ͽ� �ش� �޼ҵ带 ȣ���Ѵ�.
    /// 
    /// 1. ���� ũ�� Ȯ�� �� ������ ũ�⸦ �Ѿ��ٸ� ���� ������ Ŀ�ǵ带 initTexture�� ��ȭ��Ŵ.
    /// 2. ���ο� Ŀ�ǵ带 queue�� �����
    /// </summary>
    /// <param name="command"></param>
    public void Push(uint DrawKey)
    {
        if (DrawKeys.Count == MAX_STACK)
        {
            uint headkey = DrawKeys.First.Value;
            DrawKeys.RemoveFirst();

            DrawCommand command = null;
            if (!DrawCommands.TryGetValue(headkey, out command))
            {
                Debug.Log($"CommandStack::GetRenewedTexture : command[{headkey}] null ref.");
                return;
            }
            DrawCommands.Remove(headkey);

            DoCommand(initTexture, command);
            initTexture.Apply();
        }

        DrawKeys.AddLast(DrawKey);

        Texture2D texture = Instance.GetRenewedTexture();
        SketchScreen.Instance.RenewCanvas(texture.GetPixels());
    }

    public void Undo(uint DrawKey)
    {
        var node = DrawKeys.Find(DrawKey);
        if (node != null)
        {
            DrawKeys.Remove(node);
        }

        DrawCommands.Remove(DrawKey);

        Texture2D texture = Instance.GetRenewedTexture();
        SketchScreen.Instance.RenewCanvas(texture.GetPixels());

        return;
    }

    public Texture2D GetRenewedTexture()
    {
        Texture2D texture = new Texture2D(SketchScreen.canvasWidth, SketchScreen.canvasHeight);

        texture.SetPixels(initTexture.GetPixels());

        foreach(uint key in DrawKeys)
        {
            DrawCommand command = null;
            if(!DrawCommands.TryGetValue(key, out command))
            {
                Debug.Log($"CommandStack::GetRenewedTexture : command[{key}] null ref.");
                break;
            }

            DoCommand(texture, command);
        }

        texture.Apply();

        return texture;
    }

    private void DoCommand(Texture2D texture_, DrawCommand command)
    {
        DrawAt(ref texture_, command.vertices[0].x, command.vertices[0].y, command.DrawWidth, command.DrawColor);

        if (command.size > 1)
        {
            for (int i = 0; i < command.size - 1; i++)
            {
                Vector2Int from = command.vertices[i];
                Vector2Int to = command.vertices[i + 1];

                DrawLine(ref texture_, from, to, command.DrawWidth, command.DrawColor);
            }
        }
    }

    private void DrawAt(ref Texture2D texture_, int x, int y, float brushSize, Color color)
    {
        for (int i = -(int)brushSize; i <= brushSize; i++)
        {
            for (int j = -(int)brushSize; j <= brushSize; j++)
            {
                if (i * i + j * j <= brushSize * brushSize) // �� �ȿ� �ִ� �ȼ���
                {
                    int dx = x + i;
                    int dy = y + j;

                    if (dx >= 0 && dx < texture_.width && dy >= 0 && dy < texture_.height)
                        texture_.SetPixel(dx, dy, color);
                }
            }
        }
    }

    private void DrawLine(ref Texture2D texture_, Vector2Int from_, Vector2Int to_, float brushSize_, Color color_)
    {
        int dx = Mathf.Abs(to_.x - from_.x);
        int dy = Mathf.Abs(to_.y - from_.y);
        int steps = Mathf.Max(dx, dy);

        for (int i = 0; i <= steps; i++)
        {
            float t = steps == 0 ? 0 : (float)i / steps;
            int x = Mathf.RoundToInt(Mathf.Lerp(from_.x, to_.x, t));
            int y = Mathf.RoundToInt(Mathf.Lerp(from_.y, to_.y, t));
            DrawAt(ref texture_, x, y, brushSize_, color_);
        }
    }

    public void SetInitTexture(Color[] colors_)
    {
        initTexture.SetPixels(colors_);
    }
}
