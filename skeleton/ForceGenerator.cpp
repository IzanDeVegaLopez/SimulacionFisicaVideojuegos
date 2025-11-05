#include "ForceGenerator.hpp"
#include <iostream>


Directional_ForceGenerator::Directional_ForceGenerator(physx::PxVec3 force_direction, float force_magnitude)
	:ForceGenerator(force_magnitude), normalized_force_direction(force_direction.getNormalized())
{
}

Directional_ForceGenerator::Directional_ForceGenerator(std::string name, physx::PxVec3 force_direction, float force_magnitude)
	:ForceGenerator(name, force_magnitude), normalized_force_direction(force_direction.getNormalized()) {}


//F = masa * accel
physx::PxVec3 Directional_ForceGenerator::apply_force(GameObject const& g)
{
	if (!active) return{ 0,0,0 };
	return global_transform.q.rotate(force_magnitude*normalized_force_direction);
}

ForceGenerator::ForceGenerator(float force_magnitude)
	:force_magnitude(force_magnitude)
{
}

ForceGenerator::ForceGenerator(std::string name, float magnitude)
	:force_magnitude(magnitude), my_name(name)
{
	auto it = GameObject::force_generators_map.find(name);
	if (it != GameObject::force_generators_map.end()) {
		throw "There's already a force generator with that name";
	}
	GameObject::force_generators_map.emplace(name, this);
}

void ForceGenerator::cleanup_me()
{
	if (my_name == "-1") return;

	auto it = GameObject::force_generators_map.find(my_name);
	if (it == GameObject::force_generators_map.end()) {
		throw "There is no force generator with that name. Probably it has already been deleted";
	}
	GameObject::force_generators_map.erase(it);
}

/*
ForceGenerator::~ForceGenerator()
{
	//Si esto se está llamando es pq ya nadie tiene referencias a él
	auto it = GameObject::force_generators_map.find(my_name);
	if (it == GameObject::force_generators_map.end()) {
		throw "There is no force generator with that name. Probably it has already been deleted";
	}
	GameObject::force_generators_map.erase(it);
	
}
*/
Gravity_ForceGenerator::Gravity_ForceGenerator(physx::PxVec3 v, float mag)
	: Directional_ForceGenerator(v, mag) {}

Gravity_ForceGenerator::Gravity_ForceGenerator(std::string name, physx::PxVec3 force_direction, float mag)
	: Directional_ForceGenerator(name, force_direction, mag)
{
}

void Gravity_ForceGenerator::handle_keyboard_button_down(unsigned char key)
{
	if (key == 'g' || key=='G') {
		toggle();
	}
}

//returns the force to give the given object
physx::PxVec3 Gravity_ForceGenerator::apply_force(GameObject const& g)
{
	if (!active)return{ 0,0,0 };
	auto inv_mass = g.get_inv_mass();
	if (inv_mass < 0.005f) return { 0,0,0 };
	return Directional_ForceGenerator::apply_force(g) / inv_mass;
}

Wind_ForceGenerator::Wind_ForceGenerator(physx::PxVec3 v, float magnitude, float air_density, float avance_resistance_aerodinamic_coef)
	:Directional_ForceGenerator(v, magnitude), cd_p_medios(0.5 * air_density * avance_resistance_aerodinamic_coef)
{
}

Wind_ForceGenerator::Wind_ForceGenerator(std::string s, physx::PxVec3 v, float magnitude, float air_density, float avance_resistance_aerodinamic_coef)
	: Directional_ForceGenerator(s, v, magnitude), cd_p_medios(0.5*air_density*avance_resistance_aerodinamic_coef)
{
}

physx::PxVec3 Wind_ForceGenerator::apply_force(GameObject const& g)
{
	if (!active) return{ 0,0,0 };
	auto force = Directional_ForceGenerator::apply_force(g);
	return calculate_force(force, g.get_vel());//cd_p_medios * force * force.magnitude();
}

constexpr float k1 = 1, k2 = 0;
physx::PxVec3 Wind_ForceGenerator::calculate_force(physx::PxVec3 wind_speed, physx::PxVec3 obj_speed)
{
	physx::PxVec3 diff_vec = wind_speed - obj_speed;
	return k1 * diff_vec + k2 * diff_vec.magnitude()*diff_vec;
}

physx::PxQuat get_rotation_to(const physx::PxVec3 from, const physx::PxVec3 to) {
	physx::PxQuat q;
	physx::PxVec3 a = from.cross(to);
	q.x = a.x;
	q.y = a.y;
	q.z = a.z;
	auto from_mag = from.magnitude();
	auto to_mag = to.magnitude();
	q.w = (sqrt((from_mag * from_mag * to_mag * to_mag)) + from.dot(to));
	return q;
}

TorbellinoSencillo::TorbellinoSencillo(std::string s, physx::PxVec3 v, float magnitude,float height, float air_density, float avance_resistance_aerodinamic_coef)
	:Wind_ForceGenerator(s,v,magnitude,air_density,avance_resistance_aerodinamic_coef), height(height)
{
}

physx::PxVec3 TorbellinoSencillo::apply_force(GameObject const& g)
{
	if (!inside_area_of_influence(g)) return { 0,0,0 };

	physx::PxVec3& g_pos = g.get_global_tr().p;
	physx::PxVec3 vel_torbellino =  force_magnitude * physx::PxVec3{
				-(g_pos.z - global_transform.p.z),
		 height - (g_pos.y - global_transform.p.y),
				(g_pos.x - global_transform.p.x)
	};
	auto wind_force = calculate_force(vel_torbellino, g.get_vel());
	//std::cout << g_pos.x << " " << (50-g_pos.y) << " " << g_pos.z << " - " << global_transform.p.x << " " << global_transform.p.y << ' ' << global_transform.p.z << " --> " << vel_torbellino.x << ' ' << vel_torbellino.y << ' ' << vel_torbellino.z << '\n';

	return wind_force;
}

void TorbellinoSencillo::handle_keyboard_button_down(unsigned char key)
{
	if (key == 't'||key=='T') {
		toggle();
	}
}

bool TorbellinoSencillo::inside_area_of_influence(GameObject const& g) const
{
	//If distance is greater than 100, it does not affect
	return active && (global_transform.p - g.get_global_tr().p).magnitudeSquared() < 10000;
}

Variable_ForceGenerator::Variable_ForceGenerator(float force_magnitude,
	std::function<physx::PxVec3(float force, float time, GameObject const& self, GameObject const& g)> force_function)
	: ForceGenerator(force_magnitude), force_value_func(force_function), time_since_started(0) { }

Variable_ForceGenerator::Variable_ForceGenerator(std::string s, float force_magnitude,
	std::function<physx::PxVec3(float force_mag, float time, GameObject const& self, GameObject const& g)> force_function)
	: ForceGenerator(s,force_magnitude), force_value_func(force_function), time_since_started(0) { }

physx::PxVec3 Variable_ForceGenerator::apply_force(GameObject const& g)
{
	if (!active) return { 0,0,0 };
	return force_value_func(force_magnitude,time_since_started, *this, g);
}

void Variable_ForceGenerator::step(double dt)
{
	ForceGenerator::step(dt);
	time_since_started += dt;
}