using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// �ѹ��� �׸������� ǥ���ϴ� Ŭ����.
/// Undo�� �ѹ��� ��ҵǴ� ������,
/// �Ѱ����� ����, �Ѱ����� ũ��, �׸��� �������� ��ǥ�� �̷�����ִ�.
/// ������ ���� ũ��� ������ �����Ͽ� ���ӵ� �� ��ǥ�� �������� ǥ���ϸ� �ȴ�.
/// </summary>
public class DrawCommand
{
    public Color DrawColor;
    public float DrawWidth;
    public Vector2Int[] vertices;
    public int size;

    public DrawCommand(Color col, float width)
    {
        DrawColor = col;
        DrawWidth = width;
        vertices = new Vector2Int[2];
        size = 0;
    }

    public void Push(Vector2Int vertex)
    {
        if(size == vertices.Length)
        {
            Vector2Int[] newVector = new Vector2Int[size * 2];
            Array.Copy(vertices, newVector, vertices.Length);
            vertices = newVector;
        }
        
        vertices[size] = vertex;
        size++;

        return;
    }
}
