#!/bin/bash -x
WORKDIR=work
RUBY=ruby

SPEC_DIR="usr/share/rubygems-integration/3.1.0"
SO_DIR="usr/lib/x86_64-linux-gnu/ruby/vendor_ruby/3.1.0"
RB_DIR="usr/lib/ruby/vendor_ruby"

VERSION=$(ruby -r './lib/ninix-fmo/version.rb' -e 'print(NinixFMO::VERSION)')

sed -e "s/@VERSION@/${VERSION}/g" < utils/ninix-fmo.gemspec.in > ninix-fmo.gemspec

rake build

mkdir gem

gem install --install-dir ./gem pkg/ninix-fmo-${VERSION}.gem

mkdir ${WORKDIR}

pushd ${WORKDIR}

# TODO ruby versionの特定
mkdir -p ${SPEC_DIR}
mkdir -p ${SO_DIR}
mkdir -p ${RB_DIR}/ninix-fmo

cp -r ../debian DEBIAN

cp ../gem/specifications ${SPEC_DIR}
cp ../gem/extensions/x86_64-linux/3.1.0/ninix-fmo-${VERSION}/ninix-fmo ${SO_DIR}
cp ../gem/gems/ninix-fmo-${VERSION}/lib/ninix-fmo.rb ${RB_DIR}
cp ../gem/gems/ninix-fmo-${VERSION}/lib/ninix-fmo/version.rb ${RB_DIR}/ninix-fmo/

find usr -type f -exec md5sum {} \+ > DEBIAN/md5sums
INSTALLED_SIZE=$(du -sk usr | cut -f 1)
sed -i -e "s/@installed_size/${INSTALLED_SIZE}/g" -e "s/@version/${VERSION}/g" DEBIAN/control
popd
fakeroot dpkg-deb --build ${WORKDIR} .

#rm ninix-fmo.gemspec
#rm -r pkg gem work
