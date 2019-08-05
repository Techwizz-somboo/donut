#include "Game.h"

#include <Level.h>
#include <P3D/P3DFile.h>
#include <P3D/p3d.generated.h>
#include <Physics/WorldPhysics.h>
#include <Render/OpenGL/ShaderProgram.h>
#include <Render/StaticEntity.h>
#include <ResourceManager.h>
#include <array>
#include <iostream>

namespace Donut
{

Level::Level(WorldPhysics* worldPhysics)
{
	const auto worldVertSrc = File::ReadAll("shaders/world.vert");
	const auto worldFragSrc = File::ReadAll("shaders/world.frag");

	_worldShader  = std::make_unique<GL::ShaderProgram>(worldVertSrc, worldFragSrc);
	_worldPhysics = worldPhysics;

	std::array<std::string, 7> carFiles {
		"art/cars/mrplo_v.p3d",
		"art/cars/carhom_v.p3d",
		"art/cars/krust_v.p3d",
		"art/cars/cDuff.p3d",
		"art/cars/bart_v.p3d",
		"art/cars/snake_v.p3d",
		"art/cars/wiggu_v.p3d",
	};

	float offset = 0.0f;
	for (const auto& carFile : carFiles)
	{
		if (auto car = CompositeModel::LoadP3D(carFile))
		{
			auto transform = glm::translate(glm::mat4(1.0f), glm::vec3(240 + offset, 4.6f, -160));
			car->SetTransform(transform);
			_compositeModels.push_back(std::move(car));
			offset += 3.0f;
		}
	}
}

void Level::LoadP3D(const std::string& filename)
{
	std::string fullpath = "./art/" + filename;

	if (!std::filesystem::exists(fullpath))
	{
		std::cout << "Level not found: " << filename << "\n";
		return;
	}

	std::cout << "Loading level: " << filename << "\n";

	std::vector<std::unique_ptr<P3D::Locator2>> locators;

	const auto p3d = P3D::P3DFile(fullpath);

	const auto& root = p3d.GetRoot();
	for (const auto& chunk : root.GetChildren())
	{
		switch (chunk->GetType())
		{
		case P3D::ChunkType::Shader:
			Game::GetInstance().GetResourceManager().LoadShader(*P3D::Shader::Load(*chunk));
			break;
		case P3D::ChunkType::Texture:
			Game::GetInstance().GetResourceManager().LoadTexture(*P3D::Texture::Load(*chunk));
			break;
		case P3D::ChunkType::Set:
			Game::GetInstance().GetResourceManager().LoadSet(*P3D::Set::Load(*chunk));
			break;
		case P3D::ChunkType::StaticEntity:
		{
			_entities.emplace_back(std::make_unique<StaticEntity>(*P3D::StaticEntity::Load(*chunk)));
			break;
		}
		case P3D::ChunkType::StaticPhysics:
		{
			const auto& ent = P3D::StaticPhysics::Load(*chunk);

			auto const& volume = ent->GetCollisionObject()->GetVolume();
			if (volume != nullptr)
				_worldPhysics->AddCollisionVolume(*volume);

			break;
		}
		case P3D::ChunkType::InstancedStaticPhysics:
		{
			const auto& staticPhys = P3D::InstancedStaticPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<glm::mat4> transforms;
			P3D::P3DUtil::GetDrawables(staticPhys->GetInstanceList(), drawables, transforms);

			const auto& meshes = staticPhys->GetMeshes();
			std::map<std::string, size_t> meshesNameIndex;
			for (const auto& mesh : meshes)
			{
				meshesNameIndex.insert({ mesh->GetName(), meshesNameIndex.size() });
			}

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable  = drawables.at(i);
				const auto& transform = transforms.at(i);

				const auto& meshName = drawable->GetName();
				const auto& mesh     = meshes.at(meshesNameIndex.at(meshName));

				//auto model = std::make_unique<StaticEntity>(meshName, *mesh, transform);
				//_staticEntities.push_back(std::move(model));
			}

			break;
		}
		case P3D::ChunkType::DynamicPhysics:
		{
			const auto& dynaPhys = P3D::DynamicPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<glm::mat4> transforms;
			P3D::P3DUtil::GetDrawables(dynaPhys->GetInstanceList(), drawables, transforms);

			const auto& meshes = dynaPhys->GetMeshes();
			std::map<std::string, size_t> meshesNameIndex;
			for (const auto& mesh : meshes)
			{
				meshesNameIndex.insert({ mesh->GetName(), meshesNameIndex.size() });
			}

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable  = drawables.at(i);
				const auto& transform = transforms.at(i);

				const auto& meshName = drawable->GetName();
				const auto& mesh     = meshes.at(meshesNameIndex.at(meshName));

				// auto model = std::make_unique<StaticEntity>(meshName, *mesh, transform);
				//_staticEntities.push_back(std::move(model));
			}

			break;
		}
		case P3D::ChunkType::AnimDynamicPhysics:
		{
			const auto& dynaPhys = P3D::AnimDynamicPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<glm::mat4> transforms;
			P3D::P3DUtil::GetDrawables(dynaPhys->GetInstanceList(), drawables, transforms);

			const auto& animObjectWrapper = dynaPhys->GetAnimObjectWrapper();

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable  = drawables.at(i);
				const auto& transform = transforms.at(i);

				auto compositeModel = std::make_unique<CompositeModel>(CompositeModel_AnimObjectWrapper(*animObjectWrapper));
				compositeModel->SetTransform(transform);
				_compositeModels.push_back(std::move(compositeModel));
			}

			break;
		}
		case P3D::ChunkType::Intersect:
		{
			auto intersect = P3D::Intersect::Load(*chunk);
			_worldPhysics->AddIntersect(*intersect);

			break;
		}
		case P3D::ChunkType::WorldSphere:
		{
			_worldSphere = std::make_unique<WorldSphere>(*P3D::WorldSphere::Load(*chunk));
			break;
		}
		case P3D::ChunkType::Locator2:
		{
			locators.push_back(P3D::Locator2::Load(*chunk));
			break;
		}
		case P3D::ChunkType::FenceWrapper:
		{
			auto const& fence = P3D::FenceWrapper::Load(*chunk);
			_worldPhysics->AddP3DFence(*fence->GetFence());
		}
		default: break;
		}
	}
}

void Level::DynaLoadData(const std::string& dynaLoadData)
{
	std::vector<std::string> regionsLoad, regionsUnload, interiorsLoad, interiorsUnload;

	// todo: this will probably fuck up on an invalid string
	std::size_t prev = 0, pos;
	while ((pos = dynaLoadData.find_first_of(";:@$", prev)) != std::string::npos)
	{
		const std::string file = dynaLoadData.substr(prev, pos - prev);
		switch (dynaLoadData.at(pos))
		{
		case ';': regionsLoad.push_back(file); break;
		case ':': regionsUnload.push_back(file); break;
		case '@': interiorsLoad.push_back(file); break;
		case '$': interiorsUnload.push_back(file); break;
		}

		prev = pos + 1;
	}

	// todo: be a right laugh to thread all this!!! stick it all in a queue etc.

	// unload first
	for (auto const& region : regionsUnload)
		unloadRegion(region);

	// load in more shit
	for (auto const& region : regionsLoad)
		loadRegion(region);
}

void Level::loadRegion(const std::string& filename)
{
	std::cout << "load region: " << filename << std::endl;
}

void Level::unloadRegion(const std::string& filename)
{
	std::cout << "unload region: " << filename << std::endl;
}

void Level::Draw(glm::mat4& viewProj)
{
	_worldShader->Bind();
	_worldShader->SetUniformValue("viewProj", viewProj);

	if (_worldSphere != nullptr)
		_worldSphere->Draw(*_worldShader);

	for (const auto& ent : _entities)
	{
		// _worldShader->SetUniformValue("viewProj", viewProj * ent->GetTransform());
		ent->Draw(*_worldShader);
	}

	for (const auto& compositeModel : _compositeModels)
	{
		compositeModel->Draw(*_worldShader, viewProj, compositeModel->GetTransform());
	}
}

} // namespace Donut
