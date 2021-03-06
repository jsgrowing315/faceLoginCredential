//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// BSCredentialProvider implements ICredentialProvider, which is the main
// interface that logonUI uses to decide which tiles to display.
// In this sample, we will display one tile that uses each of the nine
// available UI controls.

#include <initguid.h>
#include "BSCredentialProvider.h"
#include "BSCredential.h"
#include "guid.h"

BSCredentialProvider::BSCredentialProvider() :
	_cRef(1),
	_pCredential(nullptr),
	_pCredProviderUserArray(nullptr)
{
	_pCredentials = nullptr;
	m_dwCredentialCount = 0;
	m_autoLogonWithDefault = true;
	DllAddRef();
}

BSCredentialProvider::~BSCredentialProvider()
{
	_ReleaseEnumeratedCredentials();
	if (_pCredProviderUserArray != nullptr)
	{
		_pCredProviderUserArray->Release();
		_pCredProviderUserArray = nullptr;
	}
	
	DllRelease();
}

// SetUsageScenario is the provider's cue that it's going to be asked for tiles
// in a subsequent call.
HRESULT BSCredentialProvider::SetUsageScenario(
	CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
	DWORD /*dwFlags*/)
{
	HRESULT hr;

	// Decide which scenarios to support here. Returning E_NOTIMPL simply tells the caller
	// that we're not designed for that scenario.
	logText("SetUsageScenario");
	switch (cpus)
	{
	case CPUS_LOGON:
	case CPUS_UNLOCK_WORKSTATION:
		// The reason why we need _fRecreateEnumeratedCredentials is because ICredentialProviderSetUserArray::SetUserArray() is called after ICredentialProvider::SetUsageScenario(),
		// while we need the ICredentialProviderUserArray during enumeration in ICredentialProvider::GetCredentialCount()
		_cpus = cpus;
		_fRecreateEnumeratedCredentials = true;
		hr = S_OK;
		break;

	case CPUS_CHANGE_PASSWORD:
	case CPUS_CREDUI:
		hr = E_NOTIMPL;
		break;

	default:
		hr = E_INVALIDARG;
		break;
	}

	return hr;
}

// SetSerialization takes the kind of buffer that you would normally return to LogonUI for
// an authentication attempt.  It's the opposite of ICredentialProviderCredential::GetSerialization.
// GetSerialization is implement by a credential and serializes that credential.  Instead,
// SetSerialization takes the serialization and uses it to create a tile.
//
// SetSerialization is called for two main scenarios.  The first scenario is in the credui case
// where it is prepopulating a tile with credentials that the user chose to store in the OS.
// The second situation is in a remote logon case where the remote client may wish to
// prepopulate a tile with a username, or in some cases, completely populate the tile and
// use it to logon without showing any UI.
//
// If you wish to see an example of SetSerialization, please see either the SampleCredentialProvider
// sample or the SampleCredUICredentialProvider sample.  [The logonUI team says, "The original sample that
// this was built on top of didn't have SetSerialization.  And when we decided SetSerialization was
// important enough to have in the sample, it ended up being a non-trivial amount of work to integrate
// it into the main sample.  We felt it was more important to get these samples out to you quickly than to
// hold them in order to do the work to integrate the SetSerialization changes from SampleCredentialProvider
// into this sample.]
HRESULT BSCredentialProvider::SetSerialization(
	_In_ CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION const * /*pcpcs*/)
{
	logText("credential provider set serialization");
	return E_NOTIMPL;
}

// Called by LogonUI to give you a callback.  Providers often use the callback if they
// some event would cause them to need to change the set of tiles that they enumerated.
HRESULT BSCredentialProvider::Advise(
	_In_ ICredentialProviderEvents * pcpe,
	_In_ UINT_PTR upAdviseContext)
{
	m_upAdviseContext = upAdviseContext;
	m_pcpe = pcpe;
	return E_NOTIMPL;
}

// Called by LogonUI when the ICredentialProviderEvents callback is no longer valid.
HRESULT BSCredentialProvider::UnAdvise()
{
	
	return E_NOTIMPL;
}

// Called by LogonUI to determine the number of fields in your tiles.  This
// does mean that all your tiles must have the same number of fields.
// This number must include both visible and invisible fields. If you want a tile
// to have different fields from the other tiles you enumerate for a given usage
// scenario you must include them all in this count and then hide/show them as desired
// using the field descriptors.
HRESULT BSCredentialProvider::GetFieldDescriptorCount(
	_Out_ DWORD *pdwCount)
{
	*pdwCount = SFI_NUM_FIELDS;
	return S_OK;
}

// Gets the field descriptor for a particular field.
HRESULT BSCredentialProvider::GetFieldDescriptorAt(
	DWORD dwIndex,
	_Outptr_result_nullonfailure_ CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR **ppcpfd)
{
	HRESULT hr;
	*ppcpfd = nullptr;

	// Verify dwIndex is a valid field.
	if ((dwIndex < SFI_NUM_FIELDS) && ppcpfd)
	{
		hr = FieldDescriptorCoAllocCopy(s_rgCredProvFieldDescriptors[dwIndex], ppcpfd);
	}
	else
	{
		hr = E_INVALIDARG;
	}

	return hr;
}

// Sets pdwCount to the number of tiles that we wish to show at this time.
// Sets pdwDefault to the index of the tile which should be used as the default.
// The default tile is the tile which will be shown in the zoomed view by default. If
// more than one provider specifies a default the last used cred prov gets to pick
// the default. If *pbAutoLogonWithDefault is TRUE, LogonUI will immediately call
// GetSerialization on the credential you've specified as the default and will submit
// that credential for authentication without showing any further UI.
HRESULT BSCredentialProvider::GetCredentialCount(
	_Out_ DWORD *pdwCount,
	_Out_ DWORD *pdwDefault,
	_Out_ BOOL *pbAutoLogonWithDefault)
{
	*pdwDefault = CREDENTIAL_PROVIDER_NO_DEFAULT;
	*pbAutoLogonWithDefault = m_autoLogonWithDefault;
	
	
	if (_fRecreateEnumeratedCredentials)
	{
		_fRecreateEnumeratedCredentials = false;
		logText("before _ReleaseEnumeratedCredentials");
		_ReleaseEnumeratedCredentials();
		logText("before _CreateEnumeratedCredentials");
		_CreateEnumeratedCredentials();
	}
	*pdwCount = m_dwCredentialCount;
	logText("get credential count");
	logNumber(m_dwCredentialCount);
	
	//*pdwCount = 1;

	return S_OK;
}

// Returns the credential at the index specified by dwIndex. This function is called by logonUI to enumerate
// the tiles.
HRESULT BSCredentialProvider::GetCredentialAt(
	DWORD dwIndex,
	_Outptr_result_nullonfailure_ ICredentialProviderCredential **ppcpc)
{
	HRESULT hr = E_INVALIDARG;
	*ppcpc = NULL;

	if ((dwIndex < m_dwCredentialCount) && ppcpc)
	{
		hr = _pCredentials[dwIndex]->QueryInterface(IID_PPV_ARGS(ppcpc));
	}

	return hr;
}

// This function will be called by LogonUI after SetUsageScenario succeeds.
// Sets the User Array with the list of users to be enumerated on the logon screen.
HRESULT BSCredentialProvider::SetUserArray(_In_ ICredentialProviderUserArray *users)
{
	if (_pCredProviderUserArray)
	{
		_pCredProviderUserArray->Release();
	}
	_pCredProviderUserArray = users;
	_pCredProviderUserArray->AddRef();
	return S_OK;
}

void BSCredentialProvider::_CreateEnumeratedCredentials()
{
	switch (_cpus)
	{
	case CPUS_LOGON:
	case CPUS_UNLOCK_WORKSTATION:
	{
		_EnumerateCredentials();
		break;
	}
	default:
		break;
	}
}

void BSCredentialProvider::_ReleaseEnumeratedCredentials()
{
	if (_pCredentials != NULL)
	{
		for (int i = 0; i < m_dwCredentialCount; i++)
		{
			
			_pCredentials[i]->Release();
			_pCredentials[i] = NULL;
		}

		delete[]_pCredentials;
		_pCredentials = NULL;
	}
	
}

HRESULT BSCredentialProvider::_EnumerateCredentials()
{
	HRESULT hr = E_UNEXPECTED;
	if (_pCredProviderUserArray != nullptr)
	{
		DWORD dwUserCount;
		_pCredProviderUserArray->GetCount(&dwUserCount);
		m_dwCredentialCount = dwUserCount;
		
		if (dwUserCount > 0)
		{
			_pCredentials = new BSCredential*[dwUserCount];
            ICredentialProviderUser *pCredUser;			
			for (int i = 0; i < dwUserCount; i++) {
				hr = _pCredProviderUserArray->GetAt(i, &pCredUser);
				if (SUCCEEDED(hr))
				{
					_pCredentials[i] = _pCredential = new(std::nothrow) BSCredential(m_upAdviseContext,m_pcpe);
					if (_pCredential != nullptr)
					{
						hr = _pCredential->Initialize(_cpus, s_rgCredProvFieldDescriptors, s_rgFieldStatePairs, pCredUser);
						if (FAILED(hr))
						{
							_pCredential->Release();
							_pCredential = nullptr;
						}
					}
					else
					{
						hr = E_OUTOFMEMORY;
					}
					pCredUser->Release();
				}
			}
		}
	}
	logText("_EnumerateCredentials");
	return hr;
}

// Boilerplate code to create our provider.
HRESULT CSample_CreateInstance(_In_ REFIID riid, _Outptr_ void **ppv)
{
	logText("CSample_CreateInstance");
	HRESULT hr;
	BSCredentialProvider *pProvider = new(std::nothrow) BSCredentialProvider();
	if (pProvider)
	{
		hr = pProvider->QueryInterface(riid, ppv);
		pProvider->Release();
	}
	else
	{
		hr = E_OUTOFMEMORY;
	}
	return hr;
}
