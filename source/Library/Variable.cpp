#include "stdafx.h"

#include "Variable.h"
#include "GameException.h"

namespace Library
{
	Variable::Variable(Effect& effect, ID3DX11EffectVariable* variable)
		: mEffect(effect), mVariable(variable), mVariableDesc(), mType(nullptr), mTypeDesc(), mName()
	{
		mVariable->GetDesc(&mVariableDesc);
		mName = mVariableDesc.Name;
		mType = mVariable->GetType();
		mType->GetDesc(&mTypeDesc);
	}

	Effect& Variable::GetEffect()
	{
		return mEffect;
	}

	ID3DX11EffectVariable* Variable::GetVariable() const
	{
		return mVariable;
	}

	const D3DX11_EFFECT_VARIABLE_DESC& Variable::VariableDesc() const
	{
		return mVariableDesc;
	}

	const D3DX11_EFFECT_TYPE_DESC& Variable::TypeDesc() const
	{
		return mTypeDesc;
	}

	ID3DX11EffectType* Variable::Type() const
	{
		return mType;
	}

	const std::string& Variable::Name() const
	{
		return mName;
	}

	Variable& Variable::operator<<(CXMMATRIX value)
	{
		ID3DX11EffectMatrixVariable* variable = mVariable->AsMatrix();
		if (variable->IsValid() == false)
		{
			throw GameException("Invalid effect variable cast.");
		}

		variable->SetMatrix(reinterpret_cast<const float*>(&value));

		return *this;
	}
	void Variable::SetResourceArray(ID3D11ShaderResourceView** value, UINT offset, UINT size)
	{
		ID3DX11EffectShaderResourceVariable* variable = mVariable->AsShaderResource();
		if (variable->IsValid() == false)
		{
			throw GameException("Invalid effect variable cast.");
		}

		variable->SetResourceArray(value, offset, size);
	}

	void Variable::SetMatrixArray(XMMATRIX* value, UINT offset, UINT size)
	{
		ID3DX11EffectMatrixVariable* variable = mVariable->AsMatrix();
		if (variable->IsValid() == false)
		{
			throw GameException("Invalid effect variable cast.");
		}

		variable->SetMatrixArray(reinterpret_cast<const float*>(value), offset, size);
	}

	Variable& Variable::operator<<(ID3D11ShaderResourceView* value)
	{
		ID3DX11EffectShaderResourceVariable* variable = mVariable->AsShaderResource();
		if (variable->IsValid() == false)
		{
			throw GameException("Invalid effect variable cast.");
		}

		variable->SetResource(value);

		return *this;
	}
	Variable& Variable::operator<<(FXMVECTOR value)
	{
		ID3DX11EffectVectorVariable* variable = mVariable->AsVector();
		if (variable->IsValid() == false)
		{
			throw GameException("Invalid effect variable cast.");
		}

		variable->SetFloatVector(reinterpret_cast<const float*>(&value));

		return *this;
	}

	Variable& Variable::operator<<(float value)
	{
		ID3DX11EffectScalarVariable* variable = mVariable->AsScalar();
		if (variable->IsValid() == false)
		{
			throw GameException("Invalid effect variable cast.");
		}

		variable->SetFloat(value);

		return *this;
	}
}
