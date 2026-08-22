#pragma once

#define DF_API_TYPE_KEY static_cast<uint32_t>(std::byteswap('DF'))

#define DF_API_VERSION_MAJOR 1
#define DF_API_VERSION_MINOR 0
#define DF_API_VERSION_PATCH 2

#define DF_API_VERSION ((DF_API_VERSION_MAJOR << 16) | (DF_API_VERSION_MINOR << 8) | DF_API_VERSION_PATCH)

namespace DismemberingFrameworkAPI
{
	struct DismembermentParams
	{
		bool forceExecution = false;
		std::string specificNode = "";
		bool ignoreArmorClass = false;
		bool noLimbImpulse = false;
		bool noSoundEffect = false;
		bool noPlayerEffect = false;
		RE::HitData* hitData = nullptr;
	};

	class DismemberingFrameworkAPI
	{
	public:
		virtual size_t GetVersion() const;

		virtual void Dismember(RE::Actor* target, const RE::BSFixedString& node, RE::Actor* aggressor = nullptr, RE::TESObjectWEAP* weapon = nullptr, const DismembermentParams* params = nullptr) const;
	
		virtual bool IsDismembered(RE::Actor* actor) const;
		
		virtual bool IsDismemberedNode(RE::Actor* actor, const RE::BSFixedString& node) const;

		virtual void PostDecapitate(RE::Actor* actor, RE::Actor* head) const;
		
		virtual void RefreshActorDismemberedState(RE::Actor* actor) const;
	};

	inline extern DismemberingFrameworkAPI* g_API = nullptr;

	inline bool LoadAPI()
	{
		if (g_API != nullptr) return true;
		SKSE::GetMessagingInterface()->Dispatch(DF_API_TYPE_KEY, (void*)&g_API, sizeof(void*), NULL);
		if (g_API) {
			return (g_API->GetVersion() == DF_API_VERSION);
		}
		return false;
	}
}
