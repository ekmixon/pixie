// Code generated by client-gen. DO NOT EDIT.

package v1alpha2

import (
	"context"
	"time"

	v1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	types "k8s.io/apimachinery/pkg/types"
	watch "k8s.io/apimachinery/pkg/watch"
	rest "k8s.io/client-go/rest"
	v1alpha2 "px.dev/pixie/src/operator/apis/nats.io/v1alpha2"
	scheme "px.dev/pixie/src/operator/vendored/nats/scheme"
)

// NatsServiceRolesGetter has a method to return a NatsServiceRoleInterface.
// A group's client should implement this interface.
type NatsServiceRolesGetter interface {
	NatsServiceRoles(namespace string) NatsServiceRoleInterface
}

// NatsServiceRoleInterface has methods to work with NatsServiceRole resources.
type NatsServiceRoleInterface interface {
	Create(ctx context.Context, natsServiceRole *v1alpha2.NatsServiceRole, opts v1.CreateOptions) (*v1alpha2.NatsServiceRole, error)
	Update(ctx context.Context, natsServiceRole *v1alpha2.NatsServiceRole, opts v1.UpdateOptions) (*v1alpha2.NatsServiceRole, error)
	Delete(ctx context.Context, name string, opts v1.DeleteOptions) error
	DeleteCollection(ctx context.Context, opts v1.DeleteOptions, listOpts v1.ListOptions) error
	Get(ctx context.Context, name string, opts v1.GetOptions) (*v1alpha2.NatsServiceRole, error)
	List(ctx context.Context, opts v1.ListOptions) (*v1alpha2.NatsServiceRoleList, error)
	Watch(ctx context.Context, opts v1.ListOptions) (watch.Interface, error)
	Patch(ctx context.Context, name string, pt types.PatchType, data []byte, opts v1.PatchOptions, subresources ...string) (result *v1alpha2.NatsServiceRole, err error)
	NatsServiceRoleExpansion
}

// natsServiceRoles implements NatsServiceRoleInterface
type natsServiceRoles struct {
	client rest.Interface
	ns     string
}

// newNatsServiceRoles returns a NatsServiceRoles
func newNatsServiceRoles(c *NatsV1alpha2Client, namespace string) *natsServiceRoles {
	return &natsServiceRoles{
		client: c.RESTClient(),
		ns:     namespace,
	}
}

// Get takes name of the natsServiceRole, and returns the corresponding natsServiceRole object, and an error if there is any.
func (c *natsServiceRoles) Get(ctx context.Context, name string, options v1.GetOptions) (result *v1alpha2.NatsServiceRole, err error) {
	result = &v1alpha2.NatsServiceRole{}
	err = c.client.Get().
		Namespace(c.ns).
		Resource("natsserviceroles").
		Name(name).
		VersionedParams(&options, scheme.ParameterCodec).
		Do(ctx).
		Into(result)
	return
}

// List takes label and field selectors, and returns the list of NatsServiceRoles that match those selectors.
func (c *natsServiceRoles) List(ctx context.Context, opts v1.ListOptions) (result *v1alpha2.NatsServiceRoleList, err error) {
	var timeout time.Duration
	if opts.TimeoutSeconds != nil {
		timeout = time.Duration(*opts.TimeoutSeconds) * time.Second
	}
	result = &v1alpha2.NatsServiceRoleList{}
	err = c.client.Get().
		Namespace(c.ns).
		Resource("natsserviceroles").
		VersionedParams(&opts, scheme.ParameterCodec).
		Timeout(timeout).
		Do(ctx).
		Into(result)
	return
}

// Watch returns a watch.Interface that watches the requested natsServiceRoles.
func (c *natsServiceRoles) Watch(ctx context.Context, opts v1.ListOptions) (watch.Interface, error) {
	var timeout time.Duration
	if opts.TimeoutSeconds != nil {
		timeout = time.Duration(*opts.TimeoutSeconds) * time.Second
	}
	opts.Watch = true
	return c.client.Get().
		Namespace(c.ns).
		Resource("natsserviceroles").
		VersionedParams(&opts, scheme.ParameterCodec).
		Timeout(timeout).
		Watch(ctx)
}

// Create takes the representation of a natsServiceRole and creates it.  Returns the server's representation of the natsServiceRole, and an error, if there is any.
func (c *natsServiceRoles) Create(ctx context.Context, natsServiceRole *v1alpha2.NatsServiceRole, opts v1.CreateOptions) (result *v1alpha2.NatsServiceRole, err error) {
	result = &v1alpha2.NatsServiceRole{}
	err = c.client.Post().
		Namespace(c.ns).
		Resource("natsserviceroles").
		VersionedParams(&opts, scheme.ParameterCodec).
		Body(natsServiceRole).
		Do(ctx).
		Into(result)
	return
}

// Update takes the representation of a natsServiceRole and updates it. Returns the server's representation of the natsServiceRole, and an error, if there is any.
func (c *natsServiceRoles) Update(ctx context.Context, natsServiceRole *v1alpha2.NatsServiceRole, opts v1.UpdateOptions) (result *v1alpha2.NatsServiceRole, err error) {
	result = &v1alpha2.NatsServiceRole{}
	err = c.client.Put().
		Namespace(c.ns).
		Resource("natsserviceroles").
		Name(natsServiceRole.Name).
		VersionedParams(&opts, scheme.ParameterCodec).
		Body(natsServiceRole).
		Do(ctx).
		Into(result)
	return
}

// Delete takes name of the natsServiceRole and deletes it. Returns an error if one occurs.
func (c *natsServiceRoles) Delete(ctx context.Context, name string, opts v1.DeleteOptions) error {
	return c.client.Delete().
		Namespace(c.ns).
		Resource("natsserviceroles").
		Name(name).
		Body(&opts).
		Do(ctx).
		Error()
}

// DeleteCollection deletes a collection of objects.
func (c *natsServiceRoles) DeleteCollection(ctx context.Context, opts v1.DeleteOptions, listOpts v1.ListOptions) error {
	var timeout time.Duration
	if listOpts.TimeoutSeconds != nil {
		timeout = time.Duration(*listOpts.TimeoutSeconds) * time.Second
	}
	return c.client.Delete().
		Namespace(c.ns).
		Resource("natsserviceroles").
		VersionedParams(&listOpts, scheme.ParameterCodec).
		Timeout(timeout).
		Body(&opts).
		Do(ctx).
		Error()
}

// Patch applies the patch and returns the patched natsServiceRole.
func (c *natsServiceRoles) Patch(ctx context.Context, name string, pt types.PatchType, data []byte, opts v1.PatchOptions, subresources ...string) (result *v1alpha2.NatsServiceRole, err error) {
	result = &v1alpha2.NatsServiceRole{}
	err = c.client.Patch(pt).
		Namespace(c.ns).
		Resource("natsserviceroles").
		Name(name).
		SubResource(subresources...).
		VersionedParams(&opts, scheme.ParameterCodec).
		Body(data).
		Do(ctx).
		Into(result)
	return
}
