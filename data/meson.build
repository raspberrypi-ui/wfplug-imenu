wui_dir = join_paths(wresource_dir, 'ui')
install_data('gmenu.ui', install_dir: wui_dir)

image_dir = join_paths(wresource_dir, 'images')
install_data('new.png', install_dir: image_dir)

schemas_dir = join_paths(share_dir, 'glib-2.0/schemas')
install_data('com.raspberrypi.wfplug-imenu.gschema.xml', install_dir: schemas_dir)
meson.add_install_script('glib-compile-schemas', schemas_dir)
