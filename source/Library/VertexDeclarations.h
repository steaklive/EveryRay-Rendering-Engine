#pragma once

#include "Common.h"

namespace Library
{
	typedef struct _VertexPosition
	{
		XMFLOAT4 Position;

		_VertexPosition() { }

		_VertexPosition(const XMFLOAT4& position)
			: Position(position) { }
	} VertexPosition;

	typedef struct _VertexPositionColor
	{
		XMFLOAT4 Position;
		XMFLOAT4 Color;

		_VertexPositionColor() { }

		_VertexPositionColor(const XMFLOAT4& position, const XMFLOAT4& color)
			: Position(position), Color(color) { }
	} VertexPositionColor;

	typedef struct _VertexPositionTexture
	{
		XMFLOAT4 Position;
		XMFLOAT2 TextureCoordinates;

		_VertexPositionTexture() { }

		_VertexPositionTexture(const XMFLOAT4& position, const XMFLOAT2& textureCoordinates)
			: Position(position), TextureCoordinates(textureCoordinates) { }
	} VertexPositionTexture;

	typedef struct _VertexPositionSize
	{
		XMFLOAT4 Position;
		XMFLOAT2 Size;

		_VertexPositionSize() { }

		_VertexPositionSize(const XMFLOAT4& position, const XMFLOAT2& size)
			: Position(position), Size(size) { }
	} VertexPositionSize;

	typedef struct _VertexPositionNormal
	{
		XMFLOAT4 Position;
		XMFLOAT3 Normal;

		_VertexPositionNormal() { }

		_VertexPositionNormal(const XMFLOAT4& position, const XMFLOAT3& normal)
			: Position(position), Normal(normal) { }
	} VertexPositionNormal;

	typedef struct _VertexPositionTextureNormalTangent
	{
		XMFLOAT4 Position;
		XMFLOAT2 TextureCoordinates;
		XMFLOAT3 Normal;
		XMFLOAT3 Tangent;

		_VertexPositionTextureNormalTangent()
		{
		}

		_VertexPositionTextureNormalTangent(XMFLOAT4 position, XMFLOAT2 textureCoordinates, XMFLOAT3 normal, XMFLOAT3 tangent)
			: Position(position), TextureCoordinates(textureCoordinates), Normal(normal), Tangent(tangent)
		{
		}

	} VertexPositionTextureNormalTangent;

	typedef struct _VertexPositionTextureNormal
	{
		XMFLOAT4 Position;
		XMFLOAT2 TextureCoordinates;
		XMFLOAT3 Normal;

		_VertexPositionTextureNormal() { }

		_VertexPositionTextureNormal(const XMFLOAT4& position, const XMFLOAT2& textureCoordinates, const XMFLOAT3& normal)
			: Position(position), TextureCoordinates(textureCoordinates), Normal(normal) { }
	} VertexPositionTextureNormal;

	typedef struct _VertexSkinnedPositionTextureNormal
	{
		XMFLOAT4 Position;
		XMFLOAT2 TextureCoordinates;
		XMFLOAT3 Normal;
		XMUINT4 BoneIndices;
		XMFLOAT4 BoneWeights;

		_VertexSkinnedPositionTextureNormal() { }

		_VertexSkinnedPositionTextureNormal(const XMFLOAT4& position, const XMFLOAT2& textureCoordinates, const XMFLOAT3& normal, const XMUINT4& boneIndices, const XMFLOAT4& boneWeights)
			: Position(position), TextureCoordinates(textureCoordinates), Normal(normal), BoneIndices(boneIndices), BoneWeights(boneWeights) { }
	} VertexSkinnedPositionTextureNormal;
}