#include <System.h>
#include <Graphics.h>
#include <console.h>
#include <nlohmann/json.hpp>

#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include "httpstuff.h"

const std::string url_base = "api.tinyfox.dev";
const std::string url_getter_img = "/img?animal=";
const std::string app_title = "Lohk's TinyFox Integrated Image Viewer";

const uint16_t menu_off = 7;
const double slideshow_time = 30.0;
const double refresh_rate = 240.0;
const double slow_refresh_rate = 30.0;

using namespace Lunaris;
using namespace AllegroCPP;

#define LINEDBUG(...) if (__dbg) { (*__dbg) << __VA_ARGS__ << "\n"; }

size_t write_at(File_client & c, const std::string & str);
std::string readsome(File_client & c);

HttpInterpret get_url(const std::string& get);
std::vector<std::string> get_options_auto(HttpInterpret* debug = nullptr);
std::string upper_first(std::string);

int main()
{
	cout << console::color::GREEN << "Loading \"" << url_base << "\" for app menu...";
	auto opts = get_options_auto();

	for (auto it = opts.begin(); it != opts.end();) {
		if (it->size() < 2) {
			it = opts.erase(it);
		}
		else {
			++it;
		}
	}

	if (opts.empty()) {
		cout << console::color::RED << "Cannot load \"" << url_base << "\" properly. List is empty or unavailable.";
		message_box("Failed to load URL", "The host seems to be unavailable!", "Could not load list of options from " + url_base, "", ALLEGRO_MESSAGEBOX_ERROR);
		//std::this_thread::sleep_for(std::chrono::seconds(3));
		return 0;
	}

	al_set_new_bitmap_flags(ALLEGRO_MAG_LINEAR | ALLEGRO_MIN_LINEAR);

	Text_log* __dbg = nullptr;
	Monitor_info moninfo;
	Display disp({ moninfo.get_width() * 0.8f, moninfo.get_height() * 0.8f }, app_title + " - Loading...", ALLEGRO_RESIZABLE, display_undefined_position);
	Event_queue evqu;
	Bitmap bmp(std::pair<int,int>{ 256, 256 });
	Timer max_disp_refresh(1.0 / refresh_rate);
	bool menu_state = true;
	bool shown_menu_m_tip = false;
	bool slideshow_auto = false;
	double last_slideshow = 0.0;
	size_t selected_img_src = 0;
	float zoomin = 1.0f, smooth_zoomin = 1.0f;
	std::pair<float,float> mouseoff = { 0.0f,0.0f };
	bool display_can_upd = true;

	evqu << disp;
	evqu << Event_mouse();
	evqu << Event_keyboard();
	evqu << max_disp_refresh;

	disp.flip();

	Menu_each_menu _men("Select", 0, {});
	{
		uint16_t _c = menu_off;
		for (const auto& i : opts) {
			_men.push(Menu_each_default(upper_first(i), _c, menu_flags::AS_CHECKBOX | ((_c++) == menu_off ? menu_flags::CHECKED : static_cast<menu_flags>(0))));
		}
	}

	// Check menu_off! (other stuff on menu must not be >= menu_off)
	Menu menu(disp, { 
		Menu_each_menu("File", 0, { 
			Menu_each_default("Toggle &menu (M)", 1),
			Menu_each_default("&Next (F5)", 4),
			Menu_each_default("&Slideshow", 3, menu_flags::AS_CHECKBOX), 
			Menu_each_default("Lower &Refresh Rate", 6, menu_flags::AS_CHECKBOX), 
			Menu_each_empty(),
			Menu_each_default("&Debug log toggle", 5, menu_flags::AS_CHECKBOX),
			Menu_each_default("&Exit app", 2)
		}), _men});
	menu.show();

	Menu popp(disp, Menu::menu_type::POPUP, {
			Menu_each_default("Toggle &menu (M)", 1),
			Menu_each_default("&Next (F5)", 4),
			Menu_each_default("&Slideshow", 3, menu_flags::AS_CHECKBOX),
			Menu_each_default("Lower &Refresh Rate", 6, menu_flags::AS_CHECKBOX),
			Menu_each_default("&Debug log toggle", 5, menu_flags::AS_CHECKBOX),
			Menu_each_default("&Exit app", 2)
		});

	evqu << menu;
	evqu << popp;

	bmp.set_as_target();
	al_clear_to_color(al_map_rgb(16, 16, 16));
	disp.set_as_target();

	const auto maketitl = [&disp](const std::string& stat = "") { if (stat.size()) disp.set_title(app_title + " - " + stat); else disp.set_title(app_title); };

	const auto _download_next_raw = [&] {
		LINEDBUG("[DOWNLOAD NEXT RAW]");

		cout << "Downloading image...";
		LINEDBUG("Downloading image...");

		if (selected_img_src >= opts.size()) throw - 1; // real bad tbh
		const std::string& sel = opts[selected_img_src];

		LINEDBUG("Image kind selected: " << sel);
		LINEDBUG("GET: " << url_getter_img << sel);

		auto dat = get_url(url_getter_img + sel);

		cout << "Loading image...";
		LINEDBUG("Loading image...");

		File_tmp dump("tinyimgXXXXXX.jpg", "wb+");// (fp.get_filepath() == "out.jpg" ? "out2.jpg" : "out.jpg");

		LINEDBUG("Temporary file created and now located at " << dump.get_filepath());
		LINEDBUG("Writing downloaded data to file...");

		dump.write(dat.get_body().data(), dat.get_body().size());
		dump.flush();
		dump.seek(0, ALLEGRO_SEEK_SET);

		LINEDBUG("File ready.");		
		cout << "File path: " << dump.get_filepath();

		LINEDBUG("Opening Bitmap from temporary file as JPG...");
		Bitmap nbmp(dump, 1024, 0, ".jpg");
		LINEDBUG((nbmp.valid() ? "Bitmap seems valid as JPG" : "Bitmap is not valid as JPG"));
		
		if (!nbmp) {
			LINEDBUG("Trying to open file as PNG...");
			Bitmap nbmp2(dump, 1024, 0, ".png");
			LINEDBUG((nbmp2.valid() ? "Bitmap seems valid as PNG" : "Bitmap is not valid as PNG"));
			if (!nbmp2) {
				LINEDBUG("Quitting this try. Failed.");
				return false;
			}
			else {
				LINEDBUG("Loaded as PNG");
				bmp = std::move(nbmp2);
			}
		}
		else {
			LINEDBUG("Loaded as JPG");
			bmp = std::move(nbmp);
		}

		cout << "New image loaded!";
		return true;
	};
	const auto download_next = [&] {
		maketitl("Downloading new image...");
		while (1) {
			try {
				LINEDBUG("Loading next image...");
				while (!_download_next_raw());
				LINEDBUG("Successfully loaded new image. Setting stuff up and showing on screen soon.");
				zoomin = 1.0f;
				mouseoff.first = mouseoff.second = 0;
				last_slideshow = al_get_time();
				maketitl();
				return;
			}
			catch (const std::exception& e) {
				LINEDBUG("Error (exception): " << e.what());
				cout << console::color::RED << "Failed to load new image: " << e.what();
			}
			catch (...) {
				LINEDBUG("Error (exception): uncaught");
				cout << console::color::RED << "Failed to load new image: UNKNOWN ERROR.";
			}
		}
	};
	const auto image_zoomcalc = [&](bool zooming_in) {
		LINEDBUG("Zoom call: " << (zooming_in ? "ZOOM_IN" : "ZOOM_OUT"));

		if (zooming_in) zoomin *= 1.23f;
		else zoomin *= 0.88f;

		if (zoomin < 0.1f) {
			zoomin = 0.1f;
		}

		LINEDBUG("Final zoom factor: " << std::to_string(zoomin));
	};

	cout << console::color::GREEN << "Ready.";
	max_disp_refresh.start();

	disp.flip();

	maketitl();
	download_next();

	while (disp.valid()) {

		if (slideshow_auto) {
			if (al_get_time() - last_slideshow > slideshow_time || last_slideshow == 0.0) {
				LINEDBUG("Slideshow time timed out, time to donwload new image...");
				download_next();
			}
			else {
				maketitl("Next image in " + std::to_string(static_cast<int>(slideshow_time - (al_get_time() - last_slideshow))) + " sec");
			}
		}

		if (std::exchange(display_can_upd, false)) {
			disp.clear_to_color();
			{
				float scale = 1.0f;
				if (1.0f * bmp.get_width() / bmp.get_height() > 1.0f * disp.get_width() / disp.get_height()) {
					scale = disp.get_width() * 1.0f / bmp.get_width();
				}
				else {
					scale = disp.get_height() * 1.0f / bmp.get_height();
				}

				bmp.draw({ mouseoff.first * smooth_zoomin + disp.get_width() * 0.5f, mouseoff.second * smooth_zoomin + disp.get_height() * 0.5f }, { bitmap_scale{scale * smooth_zoomin, scale * smooth_zoomin}, bitmap_rotate_transform{bmp.get_width() * 0.5f, bmp.get_height() * 0.5f, 0} });
			}
			disp.flip();
		}
		else al_rest(0.125f * max_disp_refresh.get_speed());
		
		while (evqu.has_event() && disp.valid()) {
			const auto ev = evqu.wait_for_event();

			switch (ev.get().type) {
			case ALLEGRO_EVENT_TIMER:
				display_can_upd |= (ev.get().timer.source == max_disp_refresh);
				if (max_disp_refresh.get_speed() == 1.0 / refresh_rate) smooth_zoomin = ((39.0f * smooth_zoomin) + zoomin) / 40.0f;
				else smooth_zoomin = ((4.0f * smooth_zoomin) + zoomin) / 5.0f;
				break;
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				LINEDBUG("Got DISPLAY_CLOSE event.");
				menu.hide();
				menu_state = false;
				disp.destroy();
				continue;
			case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
				if (ev.get().mouse.button == 2) popp.show();
				break;
			case ALLEGRO_EVENT_MOUSE_AXES:
				{
					ALLEGRO_MOUSE_STATE mst{};
					al_get_mouse_state(&mst);

					if (mst.buttons & 0b1) {
						mouseoff.first += 1.0f * ev.get().mouse.dx / smooth_zoomin;
						mouseoff.second += 1.0f * ev.get().mouse.dy / smooth_zoomin;
					}

					if (ev.get().mouse.dz) {
						image_zoomcalc(ev.get().mouse.dz > 0);
					}
				}
				break;
			case ALLEGRO_EVENT_KEY_DOWN:
				switch (ev.get().keyboard.keycode) {
				case ALLEGRO_KEY_ESCAPE:
					LINEDBUG("Key ESCAPE was pressed, zoom and position reset.");
					zoomin = 1.0f;
					mouseoff.first = mouseoff.second = 0.0f;
					break;
				case ALLEGRO_KEY_EQUALS:
				case ALLEGRO_KEY_PAD_PLUS:
					image_zoomcalc(true);
					break;
				case ALLEGRO_KEY_MINUS:
				case ALLEGRO_KEY_PAD_MINUS:
					image_zoomcalc(false);
					break;
				case ALLEGRO_KEY_M:
					if (!std::exchange(menu_state, true)) {
						LINEDBUG("Menu is now ENABLED (from key)");
						menu.show();
					}
					else {
						LINEDBUG("Menu is now DISABLED (from key)");
						menu_state = false;
						menu.hide();
					}
					break;
				case ALLEGRO_KEY_F5:
					download_next();
					break;
				case ALLEGRO_KEY_ENTER:
					{
						ALLEGRO_KEYBOARD_STATE kst{};
						al_get_keyboard_state(&kst);

						if (al_key_down(&kst, ALLEGRO_KEY_ALT)) {
							LINEDBUG("Key combo for fullscreen toggle used.");

							if (disp.get_flags() & ALLEGRO_FULLSCREEN_WINDOW) {
								disp.resize({ 0.8f * moninfo.get_width(), 0.8f * moninfo.get_height() });
								disp.set_flag(ALLEGRO_FULLSCREEN_WINDOW, false);
							}
							else {
								menu.hide();
								menu_state = false;
								disp.flip();
								disp.resize({ moninfo.get_width(), moninfo.get_height() });
								disp.set_flag(ALLEGRO_FULLSCREEN_WINDOW, true);
							}
							al_rest(0.1);
						}
					}
					break;
				}
				break;
			case ALLEGRO_EVENT_DISPLAY_RESIZE:
				LINEDBUG("Display resize event managed.");
				disp.acknowledge_resize();
				break;
			case ALLEGRO_EVENT_MENU_CLICK:
				{
					Menu_event mev(ev.get());
					switch (mev.get_id()) {
					case 1: // Hide menu for a while
						LINEDBUG("Toggle menu menu button triggered.");
						if (!std::exchange(shown_menu_m_tip, true)) message_box("You've hidden the menu!", "To show/toggle it again, press M on your keyboard or use right click menu", "", "", ALLEGRO_MESSAGEBOX_WARN, disp);
						if (!menu_state) {
							menu.show();
							menu_state = true;
						}
						else {
							menu.hide();
							menu_state = false;
						}
						break;
					case 2: // Exit
						LINEDBUG("Exit menu button triggered.");
						menu.hide();
						menu_state = false;
						disp.destroy();
						break;
					case 3: // Slideshow
						slideshow_auto = !slideshow_auto;
						last_slideshow = al_get_time();

						menu.find_id(mev.get_id()).set_flags(slideshow_auto ? menu_flags::CHECKED : static_cast<menu_flags>(0));
						popp.find_id(mev.get_id()).set_flags(slideshow_auto ? menu_flags::CHECKED : static_cast<menu_flags>(0));

						LINEDBUG("Slideshow menu button triggered. It is now: " << (slideshow_auto ? "ENABLED" : "DISABLED"));
						maketitl();
						break;
					case 4: // F5
						LINEDBUG("Next menu button triggered. New image will be loaded soon.");
						download_next();
						break;
					case 5: // debug
						if (__dbg) { delete __dbg; __dbg = nullptr; }
						else __dbg = new Text_log("Debugging panel");

						LINEDBUG("Debug window enabled (this)");

						menu.find_id(mev.get_id()).set_flags(__dbg ? menu_flags::CHECKED : static_cast<menu_flags>(0));
						popp.find_id(mev.get_id()).set_flags(__dbg ? menu_flags::CHECKED : static_cast<menu_flags>(0));
						break;
					case 6: // toggle refresh rate
						if (max_disp_refresh.get_speed() == 1.0 / slow_refresh_rate) {
							LINEDBUG("Refresh rate button pressed. Normal refresh rate set (240 Hz)");
							max_disp_refresh.set_speed(1.0 / refresh_rate);

							menu.find_id(mev.get_id()).set_flags(static_cast<menu_flags>(0));
							popp.find_id(mev.get_id()).set_flags(static_cast<menu_flags>(0));
						}
						else {
							LINEDBUG("Refresh rate button pressed. Slow refresh rate set (30 Hz)");
							max_disp_refresh.set_speed(1.0 / slow_refresh_rate);

							menu.find_id(mev.get_id()).set_flags(menu_flags::CHECKED);
							popp.find_id(mev.get_id()).set_flags(menu_flags::CHECKED);
						}
						break;
					default:
						{
							selected_img_src = static_cast<size_t>(mev.get_id()) - static_cast<size_t>(menu_off) - 1;
							LINEDBUG("Selected from list on menu: #" << std::to_string(selected_img_src));
							cout << console::color::GRAY << "Got menu event transl #" << selected_img_src;

							if (selected_img_src >= opts.size()) throw -1; // real bad tbh
							const std::string& sel = opts[selected_img_src];

							LINEDBUG("Item selected is now " << sel << ". Refreshing menu...");

							for (auto& i : opts) {
								if (i != sel) {
									menu.find(upper_first(i)).unset_flags(menu_flags::CHECKED);
								}
								else {
									menu.find(upper_first(i)).set_flags(menu_flags::CHECKED);
								}
							}

							LINEDBUG("Menu is up to date now. New image will be loaded soon.");
							download_next();
						}
						break;
					}
					
				}
				break;
			}
		}
	}

	LINEDBUG("App exit.");
	if (__dbg) delete __dbg;
	
	return 0;
}

size_t write_at(File_client& c, const std::string& str) 
{ 
	return c.write(str.c_str(), str.size());
}

std::string readsome(File_client& c) {
	c.set_timeout_read(1000); 
	std::string _f; 
	char _bf[4096]{};
	while (1) {
		c.set_timeout_read(200);
		size_t redd = c.read(_bf, sizeof(_bf));
		if (redd) _f.append(_bf, redd);
		else break;
	}
	//char _b{};
	//while (c.read(&_b, 1)) { 
	//	c.set_timeout_read(100); 
	//	_f.push_back(_b); 
	//} 
	return _f;
}

HttpInterpret get_url(const std::string& get)
{
	File_client cli(url_base, 80);
	if (cli.empty()) return {};

	write_at(cli, "GET " + get + " HTTP/1.1\r\nHost: " + url_base + "\r\nConnection: close\r\n\r\n");
	const std::string gottn = readsome(cli); // get content

	HttpInterpret hp;
	if (!hp.parse(gottn)) return {}; // parse http

	return hp;
}

std::vector<std::string> get_options_auto(HttpInterpret* debug)
{
	auto hp = get_url("/img");
	if (!hp) return {};

	nlohmann::json j = nlohmann::json::parse(hp.get_body()); // parse JSON
	if (j.empty() || !j.contains("available")) return {};

	std::vector<std::string> vec;

	for (const auto& i : j["available"]) {
		vec.push_back(i.get<std::string>());
	}

	if (debug) *debug = std::move(hp);

	return vec;
}

std::string upper_first(std::string str)
{
	if (str.size()) str[0] = (char)std::toupper(str[0]);
	return str;
}