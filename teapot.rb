
teapot_version "0.8.0"

define_project "Transform Flow" do |project|
	project.add_author "Samuel Williams"
	project.license = "MIT License"
	
	project.version = "0.1.0"
end

define_target "transform-flow" do |target|
	target.build do |environment|
		build_directory(package.path, 'source', environment)
	end
	
	target.depends :platform
	target.depends "Language/C++11"
	
	target.depends "Library/Dream"
	target.depends "Library/Dream/Imaging"
	target.depends "Library/opencv"
	
	target.provides "Library/TransformFlow" do
		append linkflags "-lTransformFlow"
	end
end

define_target "transform-flow-tests" do |target|
	target.build do |environment|
		build_directory(package.path, 'test', environment)
	end
	
	target.run do |environment|
		environment = environment.flatten
		
		Commands.run(environment[:install_prefix] + "bin/transform-flow-test-runner")
	end
	
	target.depends "Library/UnitTest"
	target.depends "Library/TransformFlow"
	
	target.provides "Test/TransformFlow"
end

define_configuration "transform-flow" do |configuration|
	configuration.public!
	
	configuration.require "dream"
	configuration.require "dream-imaging"
	configuration.require "opencv"
	
	configuration[:run] = ["Library/TransformFlow"]
end

define_configuration "local" do |configuration|
	configuration[:source] = "https://github.com/dream-framework"

	configuration.require "platforms"
	configuration.import "transform-flow"	
end

define_configuration "travis" do |configuration|
	configuration.import "local"
	
	configuration[:run] = ["Test/TransformFlow"]
end
